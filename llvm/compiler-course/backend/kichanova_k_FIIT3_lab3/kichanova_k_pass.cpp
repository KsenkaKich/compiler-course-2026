#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

using namespace llvm;

namespace {

class RecursiveFunctionInliningPass : public ModulePass {
public:
  static char ID;
  RecursiveFunctionInliningPass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }

  bool runOnModule(Module &M) override;

private:
  static constexpr unsigned MAX_INLINED_INSTRUCTIONS = 15;
  static constexpr unsigned MAX_RECURSION_DEPTH = 3;

  DenseMap<const Function *, MachineFunction *> MFMap;

  void buildFunctionMap(Module &M, MachineModuleInfo &MMI);
  bool processFunction(MachineFunction &MF);
  bool tryInline(MachineFunction &Caller, MachineBasicBlock &MBB,
                 MachineInstr &MI, unsigned Depth,
                 DenseSet<const Function *> &Stack);
  unsigned countInstructions(MachineFunction &MF) const;
};

char RecursiveFunctionInliningPass::ID = 0;

unsigned
RecursiveFunctionInliningPass::countInstructions(MachineFunction &MF) const {
  unsigned Cnt = 0;
  for (auto &BB : MF)
    for (auto &MI : BB)
      if (!MI.isDebugInstr() && !MI.isMetaInstruction())
        ++Cnt;
  return Cnt;
}

void RecursiveFunctionInliningPass::buildFunctionMap(Module &M,
                                                     MachineModuleInfo &MMI) {
  MFMap.clear();
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    if (MachineFunction *MF = MMI.getMachineFunction(F))
      MFMap[&F] = MF;
  }
}

bool RecursiveFunctionInliningPass::tryInline(
    MachineFunction &Caller, MachineBasicBlock &MBB, MachineInstr &MI,
    unsigned Depth, DenseSet<const Function *> &Stack) {
  if (MI.getOpcode() != X86::CALL64pcrel32)
    return false;

  if (MI.getNumOperands() == 0)
    return false;

  MachineOperand &Op = MI.getOperand(0);
  if (!Op.isGlobal())
    return false;

  const Function *CalleeF = dyn_cast<Function>(Op.getGlobal());
  if (!CalleeF)
    return false;

  if (Stack.count(CalleeF))
    return false;

  MachineFunction *CalleeMF = nullptr;

  if (CalleeF == &Caller.getFunction()) {
    if (Depth >= MAX_RECURSION_DEPTH)
      return false;
    CalleeMF = &Caller;
    Depth++;
  } else {
    auto It = MFMap.find(CalleeF);
    if (It == MFMap.end())
      return false;
    CalleeMF = It->second;
  }

  unsigned numInstrs = countInstructions(*CalleeMF);
  if (numInstrs > MAX_INLINED_INSTRUCTIONS)
    return false;

  Stack.insert(CalleeF);

  MachineRegisterInfo &CallerMRI = Caller.getRegInfo();
  MachineRegisterInfo &CalleeMRI = CalleeMF->getRegInfo();

  DenseMap<Register, Register> RegMap;
  SmallVector<MachineInstr *, 16> ToClone;

  for (auto &I : CalleeMF->front()) {
    if (!I.isReturn() && !I.isCall())
      ToClone.push_back(&I);
  }

  for (MachineInstr *Src : ToClone) {
    MachineInstr *NewMI = Caller.CloneMachineInstr(Src);

    for (auto &MO : NewMI->operands()) {
      if (!MO.isReg() || !MO.getReg().isVirtual())
        continue;

      Register OldR = MO.getReg();
      Register NewR;

      auto It = RegMap.find(OldR);
      if (It == RegMap.end()) {
        const TargetRegisterClass *RC = CalleeMRI.getRegClass(OldR);
        NewR = CallerMRI.createVirtualRegister(RC);
        RegMap[OldR] = NewR;
      } else {
        NewR = It->second;
      }

      MO.setReg(NewR);
    }

    MBB.insert(MI, NewMI);
  }

  MI.eraseFromParent();
  Stack.erase(CalleeF);

  return true;
}

bool RecursiveFunctionInliningPass::processFunction(MachineFunction &MF) {
  bool Changed = false;
  bool LocalChanged = true;

  while (LocalChanged) {
    LocalChanged = false;

    for (auto &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end();) {
        MachineInstr &MI = *It++;

        DenseSet<const Function *> Stack;

        if (tryInline(MF, MBB, MI, 0, Stack)) {
          Changed = true;
          LocalChanged = true;
        }
      }
    }
  }

  return Changed;
}

bool RecursiveFunctionInliningPass::runOnModule(Module &M) {
  MachineModuleInfo &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();

  buildFunctionMap(M, MMI);

  bool Changed = false;

  for (auto &KV : MFMap) {
    Changed |= processFunction(*KV.second);
  }

  return Changed;
}

} // namespace

static RegisterPass<RecursiveFunctionInliningPass>
    X("recursive-inlining", "Recursive Function Inlining Pass", false, false);