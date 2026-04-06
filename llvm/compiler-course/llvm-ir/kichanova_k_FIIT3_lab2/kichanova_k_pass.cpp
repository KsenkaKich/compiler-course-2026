#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

namespace {

struct DecomposeRemainderPass : llvm::PassInfoMixin<DecomposeRemainderPass> {
  llvm::PreservedAnalyses run(llvm::Function &func,
                              llvm::FunctionAnalysisManager &) {
    bool changed = false;

    for (auto &bb : func) {
      for (auto &instr : llvm::make_early_inc_range(bb)) {
        if (auto *binOp = llvm::dyn_cast<llvm::BinaryOperator>(&instr)) {
          auto opCode = binOp->getOpcode();

          if (opCode == llvm::Instruction::FRem) {
            auto *a = binOp->getOperand(0);
            auto *b = binOp->getOperand(1);
            llvm::IRBuilder<> builder(binOp);

            auto *div = builder.CreateFDiv(a, b);
            auto *mul = builder.CreateFMul(div, b);
            auto *result = builder.CreateFSub(a, mul);

            binOp->replaceAllUsesWith(result);
            binOp->eraseFromParent();
            changed = true;

            llvm::errs() << "decomposed frem in func: " << func.getName()
                         << "\n";
          } else if (opCode == llvm::Instruction::SRem) {
            auto *a = binOp->getOperand(0);
            auto *b = binOp->getOperand(1);
            llvm::IRBuilder<> builder(binOp);

            auto *div = builder.CreateSDiv(a, b);
            auto *mul = builder.CreateMul(div, b);
            auto *result = builder.CreateSub(a, mul);

            binOp->replaceAllUsesWith(result);
            binOp->eraseFromParent();
            changed = true;

            llvm::errs() << "decomposed srem in func: " << func.getName()
                         << "\n";
          } else if (opCode == llvm::Instruction::URem) {
            auto *a = binOp->getOperand(0);
            auto *b = binOp->getOperand(1);
            llvm::IRBuilder<> builder(binOp);

            auto *div = builder.CreateUDiv(a, b);
            auto *mul = builder.CreateMul(div, b);
            auto *result = builder.CreateSub(a, mul);

            binOp->replaceAllUsesWith(result);
            binOp->eraseFromParent();
            changed = true;

            llvm::errs() << "decomposed urem in func: " << func.getName()
                         << "\n";
          }
        }
      }
    }

    return changed ? llvm::PreservedAnalyses::none()
                   : llvm::PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DecomposeRemainderPass", "1.0",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "decompose_remainder") {
                    FPM.addPass(DecomposeRemainderPass{});
                    return true;
                  }
                  return false;
                });
          }};
}