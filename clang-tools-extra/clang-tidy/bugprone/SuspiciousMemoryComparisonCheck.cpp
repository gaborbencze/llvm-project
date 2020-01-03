//===--- SuspiciousMemoryComparisonCheck.cpp - clang-tidy -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SuspiciousMemoryComparisonCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace bugprone {

static bool hasPadding(const ASTContext &Ctx, const RecordDecl *RD,
                       uint64_t ComparedBits);

static uint64_t getFieldSize(const FieldDecl &FD, QualType FieldType,
                             const ASTContext &Ctx) {
  return FD.isBitField() ? FD.getBitWidthValue(Ctx)
                         : Ctx.getTypeSize(FieldType);
}

static bool hasPaddingInBase(const ASTContext &Ctx, const RecordDecl *RD,
                             uint64_t ComparedBits, uint64_t &TotalSize) {
  const auto IsNotEmptyBase = [](const CXXBaseSpecifier &Base) {
    return !Base.getType()->getAsCXXRecordDecl()->isEmpty();
  };

  if (const auto *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
    const auto NonEmptyBaseIt = llvm::find_if(CXXRD->bases(), IsNotEmptyBase);
    if (NonEmptyBaseIt != CXXRD->bases().end()) {
      assert(llvm::count_if(CXXRD->bases(), IsNotEmptyBase) == 1 &&
             "RD is expected to be a standard layout type");

      const CXXRecordDecl *BaseRD =
          NonEmptyBaseIt->getType()->getAsCXXRecordDecl();
      uint64_t SizeOfBase = Ctx.getTypeSize(BaseRD->getTypeForDecl());
      TotalSize += SizeOfBase;

      // Check if comparing padding in base.
      if (hasPadding(Ctx, BaseRD, ComparedBits))
        return true;
    }
  }

  return false;
}

static bool hasPaddingBetweenFields(const ASTContext &Ctx, const RecordDecl *RD,
                                    uint64_t ComparedBits,
                                    uint64_t &TotalSize) {
  for (const auto &Field : RD->fields()) {
    uint64_t FieldOffset = Ctx.getFieldOffset(Field);
    assert(FieldOffset >= TotalSize &&
           "Fields seem to overlap; this should never happen!");

    // Check if comparing padding before this field.
    if (FieldOffset > TotalSize && TotalSize < ComparedBits)
      return true;

    if (FieldOffset >= ComparedBits)
      return false;

    uint64_t SizeOfField = getFieldSize(*Field, Field->getType(), Ctx);
    TotalSize += SizeOfField;

    // Check if comparing padding in nested record.
    if (Field->getType()->isRecordType() &&
        hasPadding(Ctx, Field->getType()->getAsRecordDecl()->getDefinition(),
                   ComparedBits - FieldOffset))
      return true;
  }

  return false;
}

static bool hasPadding(const ASTContext &Ctx, const RecordDecl *RD,
                       uint64_t ComparedBits) {
  assert(RD && RD->isCompleteDefinition() &&
         "Expected the complete record definition.");
  if (RD->isUnion())
    return false;

  uint64_t TotalSize = 0;

  if (hasPaddingInBase(Ctx, RD, ComparedBits, TotalSize) ||
      hasPaddingBetweenFields(Ctx, RD, ComparedBits, TotalSize))
    return true;

  // Check if comparing trailing padding.
  return TotalSize < Ctx.getTypeSize(RD->getTypeForDecl()) &&
         ComparedBits > TotalSize;
}

static llvm::Optional<int64_t> tryEvaluateSizeExpr(const Expr *SizeExpr,
                                                   const ASTContext &Ctx) {
  Expr::EvalResult Result;
  if (SizeExpr->EvaluateAsRValue(Result, Ctx))
    return Ctx.toBits(
        CharUnits::fromQuantity(Result.Val.getInt().getExtValue()));
  return None;
}

void SuspiciousMemoryComparisonCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      callExpr(callee(namedDecl(
                   anyOf(hasName("::memcmp"), hasName("::std::memcmp")))))
          .bind("call"),
      this);
}

void SuspiciousMemoryComparisonCheck::check(
    const MatchFinder::MatchResult &Result) {
  const ASTContext &Ctx = *Result.Context;
  const auto *CE = Result.Nodes.getNodeAs<CallExpr>("call");

  const Expr *SizeExpr = CE->getArg(2);
  assert(SizeExpr != nullptr && "Third argument of memcmp is mandatory.");
  llvm::Optional<int64_t> ComparedBits = tryEvaluateSizeExpr(SizeExpr, Ctx);

  for (unsigned int i = 0; i < 2; ++i) {
    const Expr *ArgExpr = CE->getArg(i);
    QualType ArgType = ArgExpr->IgnoreImplicit()->getType();
    const Type *PointeeType = ArgType->getPointeeOrArrayElementType();
    assert(PointeeType != nullptr && "PointeeType should always be available.");

    if (PointeeType->isRecordType()) {
      const RecordDecl *RD = PointeeType->getAsRecordDecl()->getDefinition();
      if (RD != nullptr) {
        if (const auto *CXXDecl = dyn_cast<CXXRecordDecl>(RD)) {
          if (!CXXDecl->isStandardLayout()) {
            diag(CE->getBeginLoc(),
                 "comparing object representation of non-standard-layout "
                 "type %0; consider using a comparison operator instead")
                << PointeeType->getAsTagDecl()->getQualifiedNameAsString();
            break;
          }
        }

        if (ComparedBits && hasPadding(Ctx, RD, *ComparedBits)) {
          diag(CE->getBeginLoc(), "comparing padding data in type %0; "
                                  "consider comparing the fields manually")
              << PointeeType->getAsTagDecl()->getQualifiedNameAsString();
          break;
        }
      }
    }
  }
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
