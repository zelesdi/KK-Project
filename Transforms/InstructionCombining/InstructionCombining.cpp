#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


namespace {
  struct InstructionCombining : public FunctionPass {
    static char ID;
    InstructionCombining() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      return false;
    }
  };
}

char InstructionCombining::ID = 0;
static RegisterPass<InstructionCombining> X("our-i-c", "Simple Instruction Combining Pass");