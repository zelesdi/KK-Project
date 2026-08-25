#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


namespace {
  struct StrengthReduction : public FunctionPass {
    static char ID;
    StrengthReduction() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      return false;
    }
  };
}

char StrengthReduction::ID = 0;
static RegisterPass<StrengthReduction> X("our-s-r", "Simple Strength Reduction Pass");