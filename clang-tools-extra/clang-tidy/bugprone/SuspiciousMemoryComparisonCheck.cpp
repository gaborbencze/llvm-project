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

static constexpr llvm::StringLiteral CallExprName = "call";

static bool hasPadding(const clang::ASTContext &Ctx, const RecordDecl *RD,
                       uint64_t ComparedBits);

static uint64_t getFieldSize(const clang::FieldDecl &FD,
                             clang::QualType FieldType,
                             const clang::ASTContext &Ctx) {
  if (FD.isBitField())
    return FD.getBitWidthValue(Ctx);
  return Ctx.getTypeSize(FieldType);
}

static bool hasPaddingInBase(const clang::ASTContext &Ctx, const RecordDecl *RD,
                             uint64_t ComparedBits, uint64_t &TotalSize) {
  const auto IsNotEmptyBase = [](const CXXBaseSpecifier &Base) {
    return !Base.getType()->getAsCXXRecordDecl()->isEmpty();
  };

  if (const CXXRecordDecl *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
    const auto NonEmptyBaseIt = llvm::find_if(CXXRD->bases(), IsNotEmptyBase);
    if (NonEmptyBaseIt != CXXRD->bases().end()) {
      assert(llvm::count_if(CXXRD->bases(), IsNotEmptyBase) == 1 &&
             "RD is expected to be a standard layout type");

      const auto *BaseRD = NonEmptyBaseIt->getType()->getAsCXXRecordDecl();
      const uint64_t SizeOfBase = Ctx.getTypeSize(BaseRD->getTypeForDecl());
      TotalSize += SizeOfBase;

      // check if comparing padding in base
      if (hasPadding(Ctx, BaseRD, ComparedBits))
        return true;
    }
  }

  return false;
}

static bool hasPaddingBetweenFields(const clang::ASTContext &Ctx,
                                    const RecordDecl *RD, uint64_t ComparedBits,
                                    uint64_t &TotalSize) {
  for (const auto &Field : RD->fields()) {
    const uint64_t FieldOffset = Ctx.getFieldOffset(Field);
    assert(FieldOffset >= TotalSize);

    if (FieldOffset > TotalSize && TotalSize < ComparedBits)
      // Padding before this field
      return true;

    if (FieldOffset >= ComparedBits)
      return false;

    const uint64_t SizeOfField = getFieldSize(*Field, Field->getType(), Ctx);
    TotalSize += SizeOfField;

    if (Field->getType()->isRecordType()) {
      // Check if comparing padding in nested record
      if (hasPadding(Ctx, Field->getType()->getAsRecordDecl()->getDefinition(),
                     ComparedBits - FieldOffset))
        return true;
    }
  }

  return false;
}

static bool hasPadding(const clang::ASTContext &Ctx, const RecordDecl *RD,
                       uint64_t ComparedBits) {
  assert(RD && RD->isCompleteDefinition());
  if (RD->isUnion())
    return false;

  uint64_t TotalSize = 0;

  if (hasPaddingInBase(Ctx, RD, ComparedBits, TotalSize) ||
      hasPaddingBetweenFields(Ctx, RD, ComparedBits, TotalSize))
    return true;

  // check for trailing padding
  return TotalSize < Ctx.getTypeSize(RD->getTypeForDecl()) &&
         ComparedBits > TotalSize;
}

static llvm::Optional<int64_t>
tryEvaluateSizeExpr(const Expr *SizeExpr, const clang::ASTContext &Ctx) {
  clang::Expr::EvalResult Result;
  if (SizeExpr->EvaluateAsRValue(Result, Ctx))
    return Ctx.toBits(
        CharUnits::fromQuantity(Result.Val.getInt().getExtValue()));
  else
    return None;
}

void SuspiciousMemoryComparisonCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      callExpr(callee(namedDecl(
                   anyOf(hasName("::memcmp"), hasName("::std::memcmp")))))
          .bind(CallExprName),
      this);
}

void SuspiciousMemoryComparisonCheck::check(
    const MatchFinder::MatchResult &Result) {
  const clang::ASTContext &Ctx = *Result.Context;
  const auto *CE = Result.Nodes.getNodeAs<CallExpr>(CallExprName);
  assert(CE != nullptr);

  const Expr *SizeExpr = CE->getArg(2);
  assert(SizeExpr != nullptr);
  const auto ComparedBits = tryEvaluateSizeExpr(SizeExpr, Ctx);

  for (unsigned int i = 0; i < 2; ++i) {
    const Expr *ArgExpr = CE->getArg(i);
    const QualType ArgType = ArgExpr->IgnoreImplicit()->getType();
    const Type *const PointeeType = ArgType->getPointeeOrArrayElementType();
    assert(PointeeType != nullptr);

    if (PointeeType->isRecordType()) {
      const RecordDecl *RD = PointeeType->getAsRecordDecl()->getDefinition();
      if (RD != nullptr) {
        if (const CXXRecordDecl *CXXDecl = dyn_cast<CXXRecordDecl>(RD)) {
          if (!CXXDecl->isStandardLayout()) {
            diag(CE->getBeginLoc(),
                 "comparing object representation of non-standard-layout "
                 "type %0; consider using a comparison operator instead")
                << ArgType->getPointeeType();
            break;
          }
        }

        if (ComparedBits && hasPadding(Ctx, RD, *ComparedBits)) {
          diag(CE->getBeginLoc(), "comparing padding bytes");
          break;
        }
      }
    }
  }
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
