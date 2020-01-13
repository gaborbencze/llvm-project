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

static uint64_t getFieldSize(const FieldDecl &FD, QualType FieldType,
                             const ASTContext &Ctx) {
  return FD.isBitField() ? FD.getBitWidthValue(Ctx)
                         : Ctx.getTypeSize(FieldType);
}

static void markUsedBits(const ASTContext &Ctx, const RecordDecl *RD,
                         llvm::BitVector &Bits, uint64_t Offset) {
  for (const auto &Field : RD->fields()) {
    uint64_t FieldOffset = Offset + Ctx.getFieldOffset(Field);
    if (Field->getType()->isRecordType()) {
      markUsedBits(Ctx, Field->getType()->getAsRecordDecl()->getDefinition(),
                   Bits, FieldOffset);
    } else {
      uint64_t FieldSize = getFieldSize(*Field, Field->getType(), Ctx);
      Bits.set(FieldOffset, FieldOffset + FieldSize);
    }
  }
}

static llvm::BitVector getUsedBits(const ASTContext &Ctx,
                                   const RecordDecl *RD) {
  llvm::BitVector Bits(Ctx.getTypeSize(RD->getTypeForDecl()));
  markUsedBits(Ctx, RD, Bits, 0);
  return Bits;
}

static llvm::Optional<int64_t> tryEvaluateSizeExpr(const Expr *SizeExpr,
                                                   const ASTContext &Ctx) {
  Expr::EvalResult Result;
  if (SizeExpr->EvaluateAsRValue(Result, Ctx))
    return Ctx.toBits(
        CharUnits::fromQuantity(Result.Val.getInt().getExtValue()));
  return None;
}

// Returns the base of RD with non-static data members.
// If no such base exists, returns RD.
const RecordDecl *getNonEmptyBase(const RecordDecl *RD) {
  auto IsNotEmptyBase = [](const CXXBaseSpecifier &Base) {
    return !Base.getType()->getAsCXXRecordDecl()->isEmpty();
  };

  if (const auto *CXXRD = dyn_cast<CXXRecordDecl>(RD)) {
    assert(CXXRD->isStandardLayout() &&
           "Only standard-layout types are supported.");

    while (true) {
      auto NonEmptyBaseIt = llvm::find_if(CXXRD->bases(), IsNotEmptyBase);
      if (NonEmptyBaseIt == CXXRD->bases().end())
        return CXXRD;
      CXXRD = NonEmptyBaseIt->getType()->getAsCXXRecordDecl();
    }
  }

  return RD;
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
      if (const RecordDecl *RD =
              PointeeType->getAsRecordDecl()->getDefinition()) {
        if (const auto *CXXDecl = dyn_cast<CXXRecordDecl>(RD)) {
          if (!CXXDecl->isStandardLayout()) {
            diag(CE->getBeginLoc(),
                 "comparing object representation of non-standard-layout "
                 "type %0; consider using a comparison operator instead")
                << PointeeType->getAsTagDecl();
            break;
          }
        }

        int firstUnusedBit =
            getUsedBits(Ctx, getNonEmptyBase(RD)).find_first_unset();
        if (ComparedBits && firstUnusedBit != -1 &&
            firstUnusedBit < *ComparedBits) {
          diag(CE->getBeginLoc(), "comparing padding data in type %0; "
                                  "consider comparing the fields manually")
              << PointeeType->getAsTagDecl();
          break;
        }
      }
    }
  }
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
