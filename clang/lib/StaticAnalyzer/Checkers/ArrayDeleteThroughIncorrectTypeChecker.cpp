//===-- ArrayDeleteThroughIncorrectTypeChecker.cpp ----------------*- C++ -*--//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This defines a checker for the EXP51-CPP CERT rule: Do not delete an array
// through a pointer of the incorrect type
//
// This check warns when an array object is deleted through a static type that
// differs from the dynamic type of the object which results in undefined
// behavior.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CallEvent.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/DynamicType.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramStateTrait.h"

using namespace clang;
using namespace ento;

namespace {
class ArrayDeleteThroughIncorrectTypeChecker
    : public Checker<check::PreStmt<CXXDeleteExpr>> {
  mutable std::unique_ptr<BugType> BT;

  class AllocationVisitor : public BugReporterVisitor {
  public:
    AllocationVisitor() : Satisfied(false) {}
    void Profile(llvm::FoldingSetNodeID &ID) const override {
      static int X = 0;
      ID.AddPointer(&X);
    }
    PathDiagnosticPieceRef VisitNode(const ExplodedNode *N,
                                     BugReporterContext &BRC,
                                     PathSensitiveBugReport &BR) override;

  private:
    bool Satisfied;
  };

public:
  void checkPreStmt(const CXXDeleteExpr *DE, CheckerContext &C) const;
};
} // end anonymous namespace

void ArrayDeleteThroughIncorrectTypeChecker::checkPreStmt(
    const CXXDeleteExpr *DE, CheckerContext &C) const {
  if (!DE->isArrayForm())
    return;

  const Expr *DeletedObj = DE->getArgument();

  const auto *DerivedClassRegion = [&C,
                                    &DeletedObj]() -> const SymbolicRegion * {
    const MemRegion *MR = C.getSVal(DeletedObj).getAsRegion();
    if (!MR)
      return nullptr;

    return MR->getBaseRegion()->getAs<SymbolicRegion>();
  }();

  if (!DerivedClassRegion)
    return;

  const auto *StaticType = DeletedObj->getType()->getPointeeCXXRecordDecl();
  const auto *DerivedClass =
      DerivedClassRegion->getSymbol()->getType()->getPointeeCXXRecordDecl();
  if (!StaticType || !DerivedClass)
    return;

  if (StaticType->getDefinition() == DerivedClass->getDefinition())
    return;

  if (!BT)
    BT.reset(new BugType(
        this, "Deleting an array through a pointer to the incorrect type",
        categories::LogicError));

  ExplodedNode *N = C.generateNonFatalErrorNode();
  if (!N)
    return;

  auto R =
      std::make_unique<PathSensitiveBugReport>(*BT, BT->getDescription(), N);

  R->markInteresting(DerivedClassRegion);
  R->addVisitor(std::make_unique<AllocationVisitor>());
  C.emitReport(std::move(R));
}

PathDiagnosticPieceRef
ArrayDeleteThroughIncorrectTypeChecker::AllocationVisitor::VisitNode(
    const ExplodedNode *N, BugReporterContext &BRC,
    PathSensitiveBugReport &BR) {
  if (Satisfied)
    return nullptr;

  const Stmt *S = N->getStmtForDiagnostics();
  if (!S)
    return nullptr;

  const auto *NewExpr = dyn_cast<CXXNewExpr>(S);
  if (!NewExpr)
    return nullptr;

  const MemRegion *M = N->getSVal(NewExpr).getAsRegion();
  if (!M)
    return nullptr;

  if (!BR.isInteresting(M))
    return nullptr;

  Satisfied = true;

  PathDiagnosticLocation Pos(S, BRC.getSourceManager(),
                             N->getLocationContext());
  return std::make_shared<PathDiagnosticEventPiece>(Pos, "Allocated here",
                                                    true);
}

void ento::registerArrayDeleteThroughIncorrectTypeChecker(CheckerManager &mgr) {
  mgr.registerChecker<ArrayDeleteThroughIncorrectTypeChecker>();
}

bool ento::shouldRegisterArrayDeleteThroughIncorrectTypeChecker(
    const CheckerManager &mgr) {
  return true;
}
