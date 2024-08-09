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
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"

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
      R << ", callee is";
      StringRef Name = Callee->getName();
      if (auto *SubProgram = Callee->getSubprogram()) {
        if (SubProgram->isArtificial())
          R << " artificial";
      }
      if (!Name.empty())
        R << " '" << Name << "'";
      else
        R << " with unknown name";
    }
    return R;
  });
}

static void remarkAddrspaceZeroAccess(OptimizationRemarkEmitter &ORE,
                                      const Function &Caller,
                                      const Instruction &Inst) {
  ORE.emit([&] {
    OptimizationRemark R(DEBUG_TYPE, "AddrspaceZeroAccess", &Inst);
    R << "in ";
    identifyFunction(R, Caller);
    if (const IntrinsicInst *II = dyn_cast<IntrinsicInst>(&Inst)) {
      R << ", '" << II->getCalledFunction()->getName() << "' call";
    } else {
      R << ", '" << Inst.getOpcodeName() << "' instruction";
    }
    if (Inst.hasName())
      R << " ('%" << Inst.getName() << "')";
    R << " accesses memory in addrspace(0)";
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
    } else if (const CallBase *Call = dyn_cast<CallBase>(&I)) {
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
      if (const AnyMemIntrinsic *MI = dyn_cast<AnyMemIntrinsic>(Call)) {
        if (MI->getDestAddressSpace() == 0) {
          AddrspaceZeroAccesses += Direction;
          remarkAddrspaceZeroAccess(ORE, F, I);
        } else if (const AnyMemTransferInst *MT =
                       dyn_cast<AnyMemTransferInst>(MI)) {
          if (MT->getSourceAddressSpace() == 0) {
            AddrspaceZeroAccesses += Direction;
            remarkAddrspaceZeroAccess(ORE, F, I);
          }
        }
      }
    } else if (const LoadInst *Load = dyn_cast<LoadInst>(&I)) {
      if (Load->getPointerAddressSpace() == 0) {
        AddrspaceZeroAccesses += Direction;
        remarkAddrspaceZeroAccess(ORE, F, I);
      }
    } else if (const StoreInst *Store = dyn_cast<StoreInst>(&I)) {
      if (Store->getPointerAddressSpace() == 0) {
        AddrspaceZeroAccesses += Direction;
        remarkAddrspaceZeroAccess(ORE, F, I);
      }
    } else if (const AtomicRMWInst *At = dyn_cast<AtomicRMWInst>(&I)) {
      if (At->getPointerAddressSpace() == 0) {
        AddrspaceZeroAccesses += Direction;
        remarkAddrspaceZeroAccess(ORE, F, I);
      }
    } else if (const AtomicCmpXchgInst *At = dyn_cast<AtomicCmpXchgInst>(&I)) {
      if (At->getPointerAddressSpace() == 0) {
        AddrspaceZeroAccesses += Direction;
        remarkAddrspaceZeroAccess(ORE, F, I);
      }
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

static void remarkProperty(OptimizationRemarkEmitter &ORE, const Function &F,
                           StringRef Name, std::optional<int64_t> Value) {
  if (!Value)
    return;
  remarkProperty(ORE, F, Name, Value.value());
}

static std::vector<std::optional<int64_t>>
parseFnAttrAsIntegerFields(Function &F, StringRef Name, unsigned NumFields) {
  std::vector<std::optional<int64_t>> Result(NumFields);
  Attribute A = F.getFnAttribute(Name);
  if (!A.isStringAttribute())
    return Result;
  StringRef Rest = A.getValueAsString();
  for (unsigned I = 0; I < NumFields; ++I) {
    StringRef Field;
    std::tie(Field, Rest) = Rest.split(',');
    if (Field.empty())
      break;
    int64_t Val;
    if (Field.getAsInteger(0, Val)) {
      F.getContext().emitError("cannot parse integer in attribute '" + Name +
                               "': " + Field);
      break;
    }
    Result[I] = Val;
  }
  if (!Rest.empty())
    F.getContext().emitError("too many fields in attribute " + Name);
  return Result;
}

static std::optional<int64_t> parseFnAttrAsInteger(Function &F,
                                                   StringRef Name) {
  return parseFnAttrAsIntegerFields(F, Name, 1)[0];
}

// TODO: This nearly duplicates the same function in OMPIRBuilder.cpp.  Can we
// share?
static MDNode *getNVPTXMDNode(Function &F, StringRef Name) {
  Module &M = *F.getParent();
  NamedMDNode *MD = M.getNamedMetadata("nvvm.annotations");
  if (!MD)
    return nullptr;
  for (auto *Op : MD->operands()) {
    if (Op->getNumOperands() != 3)
      continue;
    auto *KernelOp = dyn_cast<ConstantAsMetadata>(Op->getOperand(0));
    if (!KernelOp || KernelOp->getValue() != &F)
      continue;
    auto *Prop = dyn_cast<MDString>(Op->getOperand(1));
    if (!Prop || Prop->getString() != Name)
      continue;
    return Op;
  }
  return nullptr;
}

static std::optional<int64_t> parseNVPTXMDNodeAsInteger(Function &F,
                                                        StringRef Name) {
  std::optional<int64_t> Result;
  if (MDNode *ExistingOp = getNVPTXMDNode(F, Name)) {
    auto *Op = cast<ConstantAsMetadata>(ExistingOp->getOperand(2));
    Result = cast<ConstantInt>(Op->getValue())->getZExtValue();
  }
  return Result;
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

  // Record function properties.
  KI.ExternalNotKernel = F.hasExternalLinkage() && !isKernelFunction(F);
  KI.OmpTargetNumTeams = parseFnAttrAsInteger(F, "omp_target_num_teams");
  KI.OmpTargetThreadLimit = parseFnAttrAsInteger(F, "omp_target_thread_limit");
  auto AmdgpuMaxNumWorkgroups =
      parseFnAttrAsIntegerFields(F, "amdgpu-max-num-workgroups", 3);
  KI.AmdgpuMaxNumWorkgroupsX = AmdgpuMaxNumWorkgroups[0];
  KI.AmdgpuMaxNumWorkgroupsY = AmdgpuMaxNumWorkgroups[1];
  KI.AmdgpuMaxNumWorkgroupsZ = AmdgpuMaxNumWorkgroups[2];
  auto AmdgpuFlatWorkGroupSize =
      parseFnAttrAsIntegerFields(F, "amdgpu-flat-work-group-size", 2);
  KI.AmdgpuFlatWorkGroupSizeMin = AmdgpuFlatWorkGroupSize[0];
  KI.AmdgpuFlatWorkGroupSizeMax = AmdgpuFlatWorkGroupSize[1];
  auto AmdgpuWavesPerEu =
      parseFnAttrAsIntegerFields(F, "amdgpu-waves-per-eu", 2);
  KI.AmdgpuWavesPerEuMin = AmdgpuWavesPerEu[0];
  KI.AmdgpuWavesPerEuMax = AmdgpuWavesPerEu[1];
  KI.Maxclusterrank = parseNVPTXMDNodeAsInteger(F, "maxclusterrank");
  KI.Maxntidx = parseNVPTXMDNodeAsInteger(F, "maxntidx");

  const DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
  auto &ORE = FAM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  for (const auto &BB : F)
    if (DT.isReachableFromEntry(&BB))
      KI.updateForBB(BB, +1, ORE);

#define REMARK_PROPERTY(PROP_NAME)                                             \
  remarkProperty(ORE, F, #PROP_NAME, KI.PROP_NAME)
  REMARK_PROPERTY(ExternalNotKernel);
  REMARK_PROPERTY(OmpTargetNumTeams);
  REMARK_PROPERTY(OmpTargetThreadLimit);
  REMARK_PROPERTY(AmdgpuMaxNumWorkgroupsX);
  REMARK_PROPERTY(AmdgpuMaxNumWorkgroupsY);
  REMARK_PROPERTY(AmdgpuMaxNumWorkgroupsZ);
  REMARK_PROPERTY(AmdgpuFlatWorkGroupSizeMin);
  REMARK_PROPERTY(AmdgpuFlatWorkGroupSizeMax);
  REMARK_PROPERTY(AmdgpuWavesPerEuMin);
  REMARK_PROPERTY(AmdgpuWavesPerEuMax);
  REMARK_PROPERTY(Maxclusterrank);
  REMARK_PROPERTY(Maxntidx);
  REMARK_PROPERTY(Allocas);
  REMARK_PROPERTY(AllocasStaticSizeSum);
  REMARK_PROPERTY(AllocasDyn);
  REMARK_PROPERTY(DirectCalls);
  REMARK_PROPERTY(IndirectCalls);
  REMARK_PROPERTY(DirectCallsToDefinedFunctions);
  REMARK_PROPERTY(Invokes);
  REMARK_PROPERTY(AddrspaceZeroAccesses);
#undef REMARK_PROPERTY

  return KI;
}

AnalysisKey KernelInfoAnalysis::Key;
