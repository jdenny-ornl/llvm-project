//===- KernelInfo.cpp - Kernel Analysis -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the KernelInfo, KernelInfoAnalysis, and KernelInfoPrinter
// classes used to extract function properties from a kernel.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/KernelInfo.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

using namespace llvm;

#define DEBUG_TYPE "kernel-info"

static bool isKernelFunction(Function &F) {
  // TODO: Is this general enough?  Consider languages beyond OpenMP.
  return F.hasFnAttribute("kernel");
}

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
    if (Name.empty()) {
      R << "unnamed alloca ";
      if (DVRs.empty())
        R << "(missing debug metadata) ";
    } else {
      R << "alloca '" << Name << "' ";
    }
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
      Allocas += Direction;
      TypeSize::ScalarTy StaticSize = 0;
      if (std::optional<TypeSize> Size = Alloca->getAllocationSize(DL)) {
        StaticSize = Size->getFixedValue();
        assert(StaticSize <= std::numeric_limits<int64_t>::max());
        AllocasStaticSizeSum += Direction * StaticSize;
      } else {
        AllocasDyn += Direction;
      }
      remarkAlloca(ORE, F, *Alloca, StaticSize);
    } else if (const auto *Call = dyn_cast<CallBase>(&I)) {
      std::string CallKind;
      std::string RemarkKind;
      if (Call->isIndirectCall()) {
        IndirectCalls += Direction;
        CallKind += "indirect";
        RemarkKind += "Indirect";
      } else {
        DirectCalls += Direction;
        CallKind += "direct";
        RemarkKind += "Direct";
      }
      if (isa<InvokeInst>(Call)) {
        Invokes += Direction;
        CallKind += " invoke";
        RemarkKind += "Invoke";
      } else {
        CallKind += " call";
        RemarkKind += "Call";
      }
      if (!Call->isIndirectCall()) {
        if (const Function *Callee = Call->getCalledFunction()) {
          if (Callee && !Callee->isIntrinsic() && !Callee->isDeclaration()) {
            DirectCallsToDefinedFunctions += Direction;
            CallKind += " to defined function";
            RemarkKind += "ToDefinedFunction";
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

  // Report potentially problematic linkage.
  auto &ORE = FAM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  KI.ExternalNotKernel = F.hasExternalLinkage() && !isKernelFunction(F);

  const DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
  for (const auto &BB : F)
    if (DT.isReachableFromEntry(&BB))
      KI.updateForBB(BB, +1, ORE);

#define REMARK_PROPERTY(PROP_NAME)                                             \
  remarkProperty(ORE, F, #PROP_NAME, KI.PROP_NAME)
  REMARK_PROPERTY(ExternalNotKernel);
  REMARK_PROPERTY(Allocas);
  REMARK_PROPERTY(AllocasStaticSizeSum);
  REMARK_PROPERTY(AllocasDyn);
  REMARK_PROPERTY(DirectCalls);
  REMARK_PROPERTY(IndirectCalls);
  REMARK_PROPERTY(DirectCallsToDefinedFunctions);
  REMARK_PROPERTY(Invokes);
#undef REMARK_PROPERTY

  return KI;
}

AnalysisKey KernelInfoAnalysis::Key;

llvm::PassPluginLibraryInfo getKernelInfoPluginInfo() {
  using namespace llvm;
  return {
      LLVM_PLUGIN_API_VERSION, "KernelInfoPrinter", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        // Enables: AM.getResult<KernelInfoAnalysis>(F)
        PB.registerAnalysisRegistrationCallback(
            [](llvm::FunctionAnalysisManager &PM) {
              PM.registerPass([&] { return KernelInfoAnalysis(); });
            });
        // Enables: opt -passes=kernel-info
        PB.registerPipelineParsingCallback(
            [&](StringRef Name, FunctionPassManager &FPM,
                ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "kernel-info") {
                FPM.addPass(KernelInfoPrinter());
                return true;
              }
              return false;
            });
        // Insert into pipeline formed by, e.g., opt -passes='default<O1>'.
        PB.registerScalarOptimizerLateEPCallback(
            [](llvm::FunctionPassManager &PM, llvm::OptimizationLevel Level) {
              PM.addPass(KernelInfoPrinter());
            });
      }};
}

// Used when built as dynamic plugin.
#ifndef LLVM_KERNELINFO_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getKernelInfoPluginInfo();
}
#endif
