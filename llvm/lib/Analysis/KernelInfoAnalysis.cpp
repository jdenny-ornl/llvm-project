//===- KernelInfoAnalysis.cpp - Kernel Analysis ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the KernelInfo and KernelInfoAnalysis classes used to
// extract function properties from a kernel.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/KernelInfoAnalysis.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;

#define DEBUG_TYPE "kernel-info"

static void identifyFunction(OptimizationRemark &R, const Function &F) {
  if (auto *SubProgram = F.getSubprogram()) {
    if (SubProgram->isArtificial())
      R << "artificial ";
  }
  R << "function '" << F.getName() << "'";
}

static void remarkAlloca(OptimizationRemarkEmitter &ORE, const Function &Caller,
                         const AllocaInst &Alloca,
                         TypeSize::ScalarTy StaticSize) {
  ORE.emit([&] {
    StringRef Name;
    DebugLoc Loc;
    bool Artificial = false;
    auto DVRs = findDVRDeclares(&const_cast<AllocaInst &>(Alloca));
    if (!DVRs.empty()) {
      const DbgVariableRecord &DVR = **DVRs.begin();
      Name = DVR.getVariable()->getName();
      Loc = DVR.getDebugLoc();
      Artificial = DVR.Variable->isArtificial();
    }
    OptimizationRemark R(DEBUG_TYPE, "Alloca", DiagnosticLocation(Loc),
                         Alloca.getParent());
    R << "in ";
    identifyFunction(R, Caller);
    R << ", ";
    if (Artificial)
      R << "artificial ";
    if (Name.empty())
      R << "unnamed alloca ";
    else
      R << "alloca '" << Name << "' ";
    R << "with ";
    if (StaticSize)
      R << "static size of " << itostr(StaticSize) << " bytes";
    else
      R << "dynamic size";
    return R;
  });
}

static void remarkCall(OptimizationRemarkEmitter &ORE, const Function &Caller,
                       const CallBase &Call, StringRef CallKind,
                       StringRef RemarkKind) {
  ORE.emit([&] {
    OptimizationRemark R(DEBUG_TYPE, RemarkKind, &Call);
    R << "in ";
    identifyFunction(R, Caller);
    R << ", " << CallKind;
    if (const Function *Callee =
            dyn_cast_or_null<Function>(Call.getCalledOperand())) {
      R << ", callee is ";
      StringRef Name = Callee->getName();
      if (auto *SubProgram = Callee->getSubprogram()) {
        if (SubProgram->isArtificial())
          R << "artificial";
      }
      if (!Name.empty())
        R << " '" << Name << "'";
      else
        R << " with unknown name";
    }
    return R;
  });
}

void KernelInfo::updateForBB(const BasicBlock &BB, int64_t Direction,
                             OptimizationRemarkEmitter &ORE) {
  assert(Direction == 1 || Direction == -1);
  const Function &F = *BB.getParent();
  const Module &M = *F.getParent();
  const DataLayout &DL = M.getDataLayout();
  for (const Instruction &I : BB.instructionsWithoutDebug()) {
    if (const AllocaInst *Alloca = dyn_cast<AllocaInst>(&I)) {
      AllocaCount += Direction;
      TypeSize::ScalarTy StaticSize = 0;
      if (std::optional<TypeSize> Size = Alloca->getAllocationSize(DL)) {
        StaticSize = Size->getFixedValue();
        assert(StaticSize <= std::numeric_limits<int64_t>::max());
        AllocaStaticSizeSum += Direction * StaticSize;
      } else {
        AllocaDynCount += Direction;
      }
      remarkAlloca(ORE, F, *Alloca, StaticSize);
    } else if (const auto *Call = dyn_cast<CallBase>(&I)) {
      StringRef CallKind;
      StringRef RemarkKind;
      if (Call->isIndirectCall()) {
        IndirectCallCount += Direction;
        CallKind = "indirect call";
        RemarkKind = "IndirectCall";
      } else {
        DirectCallCount += Direction;
        CallKind = "direct call";
        RemarkKind = "DirectCall";
        if (const Function *Callee = Call->getCalledFunction()) {
          if (Callee && !Callee->isIntrinsic() && !Callee->isDeclaration()) {
            DirectCallsToDefinedFunctions += Direction;
            CallKind = "direct call to defined function";
            RemarkKind = "DirectCallToDefinedFunction";
          }
        }
      }
      remarkCall(ORE, F, *Call, CallKind, RemarkKind);
    }
  }
}

static void remarkProperty(OptimizationRemarkEmitter &ORE, const Function &F,
                           StringRef Name, int64_t Value) {
  ORE.emit([&] {
    OptimizationRemark R(DEBUG_TYPE, Name, &F);
    R << "in ";
    identifyFunction(R, F);
    R << ", " << Name << " = " << itostr(Value);
    return R;
  });
}

KernelInfo KernelInfo::getKernelInfo(Function &F,
                                     FunctionAnalysisManager &FAM) {
  KernelInfo KI;
  // Only analyze modules for GPUs.
  // TODO: This would be more maintainable if there were an isGPU.
  const std::string &TT = F.getParent()->getTargetTriple();
  llvm::Triple T(TT);
  if (!T.isAMDGPU() && !T.isNVPTX())
    return KI;
  KI.IsValid = true;

  const DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
  auto &ORE = FAM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  for (const auto &BB : F)
    if (DT.isReachableFromEntry(&BB))
      KI.updateForBB(BB, +1, ORE);

#define REMARK_PROPERTY(PROP_NAME)                                             \
  remarkProperty(ORE, F, #PROP_NAME, KI.PROP_NAME)
  REMARK_PROPERTY(AllocaCount);
  REMARK_PROPERTY(AllocaStaticSizeSum);
  REMARK_PROPERTY(AllocaDynCount);
  REMARK_PROPERTY(DirectCallCount);
  REMARK_PROPERTY(IndirectCallCount);
  REMARK_PROPERTY(DirectCallsToDefinedFunctions);
#undef REMARK_PROPERTY

  return KI;
}

AnalysisKey KernelInfoAnalysis::Key;
