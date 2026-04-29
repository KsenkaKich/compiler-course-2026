#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"

using namespace llvm;

namespace {

class InlineRecursivePass : public MachineFunctionPass {
public:
  static char ID;
  InlineRecursivePass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  static constexpr unsigned MaxInlineInstrs = 15;
  static constexpr unsigned MaxRecDepth = 3;

  DenseMap<const Function *, unsigned> RecDepth;

  bool tryInline(MachineFunction &Caller, MachineBasicBlock &MBB,
                 MachineInstr &MI);

  bool isInlineCandidate(MachineFunction &MF);

  void remapRegisters(MachineInstr *MI, DenseMap<Register, Register> &VRegMap,
                      MachineRegisterInfo &MRI);
};

char InlineRecursivePass::ID = 0;

bool InlineRecursivePass::isInlineCandidate(MachineFunction &MF) {
  unsigned Cnt = 0;
  for (auto &BB : MF) {
    for (auto &MI : BB) {
      if (!MI.isDebugInstr() && !MI.isMetaInstruction() && !MI.isLabel()) {
        ++Cnt;
        if (Cnt > MaxInlineInstrs)
          return false;
      }
    }
  }
  return Cnt <= MaxInlineInstrs;
}

void InlineRecursivePass::remapRegisters(MachineInstr *MI,
                                         DenseMap<Register, Register> &VRegMap,
                                         MachineRegisterInfo &MRI) {
  for (MachineOperand &MO : MI->operands()) {
    if (!MO.isReg())
      continue;

    Register R = MO.getReg();
    if (!R.isVirtual())
      continue;

    auto It = VRegMap.find(R);
    if (It != VRegMap.end()) {
      MO.setReg(It->second);
    }
  }
}

bool InlineRecursivePass::tryInline(MachineFunction &Caller,
                                    MachineBasicBlock &MBB, MachineInstr &MI) {
  if (MI.getOpcode() != X86::CALL64pcrel32 &&
      MI.getOpcode() != X86::CALLpcrel32)
    return false;

  if (MI.getNumOperands() == 0)
    return false;

  MachineOperand &Op = MI.getOperand(0);
  if (!Op.isGlobal())
    return false;

  const Function *CalleeF = dyn_cast<Function>(Op.getGlobal());
  if (!CalleeF)
    return false;

  if (RecDepth[CalleeF] >= MaxRecDepth)
    return false;

  MachineFunction *CalleeMF = nullptr;
  if (CalleeF == &Caller.getFunction()) {
    CalleeMF = &Caller;
  } else {
    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
    CalleeMF = MMI.getMachineFunction(*CalleeF);
    if (!CalleeMF)
      return false;
  }

  if (!isInlineCandidate(*CalleeMF))
    return false;

  ++RecDepth[CalleeF];

  MachineRegisterInfo &MRI = Caller.getRegInfo();
  DenseMap<Register, Register> VRegMap;
  DenseMap<MachineBasicBlock *, MachineBasicBlock *> BBMap;

  auto InsertPos = MI.getIterator();
  MachineBasicBlock *CallBlock = &MBB;
  const TargetInstrInfo *TII = Caller.getSubtarget().getInstrInfo();

  MachineBasicBlock *NextBlock = nullptr;
  auto NextIt = std::next(InsertPos);
  if (NextIt != CallBlock->end()) {
    NextBlock = NextIt->getParent();
  } else {
    auto MBBI = std::next(CallBlock->getIterator());
    if (MBBI != Caller.end()) {
      NextBlock = &*MBBI;
    }
  }

  Register ResultReg;
  if (MI.getNumOperands() > 1 && MI.getOperand(1).isReg() &&
      MI.getOperand(1).isDef()) {
    ResultReg = MI.getOperand(1).getReg();
  }

  SmallVector<MachineBasicBlock *, 8> WorkList;
  for (auto &BB : *CalleeMF) {
    WorkList.push_back(&BB);
  }

  for (MachineBasicBlock *BB : WorkList) {
    MachineBasicBlock *NewBB = Caller.CreateMachineBasicBlock();
    Caller.insert(CallBlock->getIterator(), NewBB);
    BBMap[BB] = NewBB;
  }

  for (MachineBasicBlock *BB : WorkList) {
    MachineBasicBlock *NewBB = BBMap[BB];

    for (auto &MI_Iter : *BB) {
      if (MI_Iter.isPHI() || MI_Iter.isTerminator())
        continue;

      MachineInstr *NewMI = Caller.CloneMachineInstr(&MI_Iter);
      remapRegisters(NewMI, VRegMap, MRI);
      NewBB->push_back(NewMI);
    }

    for (auto &MI_Iter : BB->terminators()) {
      if (MI_Iter.isReturn()) {
        if (NextBlock) {
          BuildMI(NewBB, DebugLoc(), TII->get(X86::JMP_1)).addMBB(NextBlock);
        }
      } else {
        MachineInstr *NewMI = Caller.CloneMachineInstr(&MI_Iter);
        remapRegisters(NewMI, VRegMap, MRI);

        for (MachineOperand &MO : NewMI->operands()) {
          if (MO.isMBB() && BBMap.count(MO.getMBB())) {
            MO.setMBB(BBMap[MO.getMBB()]);
          }
        }
        NewBB->push_back(NewMI);
      }
    }

    for (MachineBasicBlock *Succ : BB->successors()) {
      MachineBasicBlock *NewSucc = BBMap[Succ];
      for (MachineInstr &PHI : NewSucc->phis()) {
        for (unsigned i = 2; i < PHI.getNumOperands(); i += 2) {
          if (PHI.getOperand(i).getMBB() == BB) {
            PHI.getOperand(i).setMBB(NewBB);
          }
        }
      }
    }
  }

  if (ResultReg.isValid()) {
    Register RetReg = 0;
    for (auto &BB : *CalleeMF) {
      for (auto &MI_Iter : BB) {
        if (MI_Iter.isReturn() && MI_Iter.getNumOperands() > 0) {
          RetReg = MI_Iter.getOperand(0).getReg();
          break;
        }
      }
      if (RetReg != 0)
        break;
    }

    if (RetReg != 0 && VRegMap.count(RetReg)) {
      Register NewRetReg = VRegMap[RetReg];
      if (NextBlock) {
        BuildMI(*NextBlock, NextBlock->begin(), DebugLoc(),
                TII->get(TargetOpcode::COPY), ResultReg)
            .addReg(NewRetReg);
      } else {
        BuildMI(*CallBlock, InsertPos, DebugLoc(), TII->get(TargetOpcode::COPY),
                ResultReg)
            .addReg(NewRetReg);
      }
    }
  }

  MI.eraseFromParent();

  --RecDepth[CalleeF];
  return true;
}

bool InlineRecursivePass::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  bool LocalChanged = true;

  while (LocalChanged) {
    LocalChanged = false;

    for (auto &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end();) {
        MachineInstr &MI = *It++;
        if (tryInline(MF, MBB, MI)) {
          LocalChanged = true;
          Changed = true;
        }
      }
    }
  }
  return Changed;
}

} // namespace

static RegisterPass<InlineRecursivePass>
    X("inline-recursive", "Recursive Function Inlining Pass", false, false);