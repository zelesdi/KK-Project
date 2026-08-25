#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


namespace {
  struct DeadArgumentElimination : public FunctionPass {
    static char ID;
    DeadArgumentElimination() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      return false;
    }
  };
}

char DeadArgumentElimination::ID = 0;
static RegisterPass<DeadArgumentElimination> X("our-d-a-elim", "Simple Dead Argument Elimination Pass");