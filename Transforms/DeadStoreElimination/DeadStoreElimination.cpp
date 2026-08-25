#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


namespace {
  struct DeadStoreElimination : public FunctionPass {
    static char ID;
    DeadStoreElimination() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      return false;
    }
  };
}

char DeadStoreElimination::ID = 0;
static RegisterPass<DeadStoreElimination> X("our-ds-elim", "Simple Dead Store Elimination Pass");