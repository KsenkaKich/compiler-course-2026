#include "mlir/Pass/Pass.h"
#include "mlir/Dialect/Affine/IR/AffineOps.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace {
class TraceConditionPass
    : public PassWrapper<TraceConditionPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "trace-condition"; }
  StringRef getDescription() const final {
    return "inserts trace function calls on each condition block";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();

    createTraceFunctions(moduleOp);

    int thenBeginCount = 0, thenEndCount = 0, elseBeginCount = 0,
        elseEndCount = 0;

    moduleOp.walk([&](Operation *op) {
      if (auto ifOp = dyn_cast<scf::IfOp>(op)) {
        if (processSCFIfOp(ifOp)) {
          thenBeginCount++;
          thenEndCount++;

          if (ifOp.getNumRegions() > 1 && !ifOp.getElseRegion().empty()) {
            elseBeginCount++;
            elseEndCount++;
          }
        }
      } else if (auto affineIfOp = dyn_cast<affine::AffineIfOp>(op)) {
        if (processAffineIfOp(affineIfOp)) {
          thenBeginCount++;
          thenEndCount++;
          if (affineIfOp.hasElse()) {
            elseBeginCount++;
            elseEndCount++;
          }
        }
      }
    });

    llvm::outs() << "trace_condition_then_begin: " << thenBeginCount << "\n";
    llvm::outs() << "trace_condition_then_end: " << thenEndCount << "\n";
    llvm::outs() << "trace_condition_else_begin: " << elseBeginCount << "\n";
    llvm::outs() << "trace_condition_else_end: " << elseEndCount << "\n";
  }

private:
  void createTraceFunctions(ModuleOp moduleOp) {
    OpBuilder builder(moduleOp.getContext());
    builder.setInsertionPointToStart(moduleOp.getBody());

    auto funcType = FunctionType::get(builder.getContext(), {}, {});

    SmallVector<std::string, 4> traceFuncNames = {
        "trace_condition_then_begin", "trace_condition_then_end",
        "trace_condition_else_begin", "trace_condition_else_end"};

    for (const auto &funcName : traceFuncNames) {
      if (!moduleOp.lookupSymbol<func::FuncOp>(funcName)) {
        func::FuncOp func =
            builder.create<func::FuncOp>(moduleOp.getLoc(), funcName, funcType);
        func.setPrivate();
      }
    }
  }

  bool processSCFIfOp(scf::IfOp ifOp) {
    OpBuilder builder(ifOp.getContext());
    bool modified = false;

    Block *thenBlock = ifOp.thenBlock();
    if (thenBlock && !thenBlock->empty()) {
      builder.setInsertionPointToStart(thenBlock);
      builder.create<func::CallOp>(ifOp.getLoc(), "trace_condition_then_begin",
                                   TypeRange{});
      modified = true;

      Operation *terminator = thenBlock->getTerminator();
      if (terminator) {
        builder.setInsertionPoint(terminator);
        builder.create<func::CallOp>(ifOp.getLoc(), "trace_condition_then_end",
                                     TypeRange{});
        modified = true;
      }
    }

    if (ifOp.getNumRegions() > 1 && !ifOp.getElseRegion().empty()) {
      Block *elseBlock = ifOp.elseBlock();
      if (elseBlock && !elseBlock->empty()) {
        builder.setInsertionPointToStart(elseBlock);
        builder.create<func::CallOp>(ifOp.getLoc(),
                                     "trace_condition_else_begin", TypeRange{});
        modified = true;

        Operation *terminator = elseBlock->getTerminator();
        if (terminator) {
          builder.setInsertionPoint(terminator);
          builder.create<func::CallOp>(ifOp.getLoc(),
                                       "trace_condition_else_end", TypeRange{});
          modified = true;
        }
      }
    }
    return modified;
  }

  bool processAffineIfOp(affine::AffineIfOp affineIfOp) {
    OpBuilder builder(affineIfOp.getContext());
    bool modified = false;

    Block *thenBlock = affineIfOp.getThenBlock();
    if (thenBlock && !thenBlock->empty()) {
      builder.setInsertionPointToStart(thenBlock);
      builder.create<func::CallOp>(affineIfOp.getLoc(),
                                   "trace_condition_then_begin", TypeRange{});
      modified = true;

      Operation *terminator = thenBlock->getTerminator();
      if (terminator) {
        builder.setInsertionPoint(terminator);
        builder.create<func::CallOp>(affineIfOp.getLoc(),
                                     "trace_condition_then_end", TypeRange{});
        modified = true;
      }
    }

    if (affineIfOp.hasElse()) {
      Block *elseBlock = affineIfOp.getElseBlock();
      if (elseBlock && !elseBlock->empty()) {
        builder.setInsertionPointToStart(elseBlock);
        builder.create<func::CallOp>(affineIfOp.getLoc(),
                                     "trace_condition_else_begin", TypeRange{});
        modified = true;

        Operation *terminator = elseBlock->getTerminator();
        if (terminator) {
          builder.setInsertionPoint(terminator);
          builder.create<func::CallOp>(affineIfOp.getLoc(),
                                       "trace_condition_else_end", TypeRange{});
          modified = true;
        }
      }
    }
    return modified;
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(TraceConditionPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(TraceConditionPass)

mlir::PassPluginLibraryInfo getTraceConditionPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "TraceConditionPass", "1.0",
          []() { mlir::PassRegistration<TraceConditionPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getTraceConditionPassPluginInfo();
}