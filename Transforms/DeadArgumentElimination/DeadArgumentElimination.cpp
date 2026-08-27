#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


namespace {
  struct DeadArgumentElimination : public FunctionPass {
    std::vector<Type *> argTypes;

    static char ID;
    DeadArgumentElimination() : FunctionPass(ID) {}

    void findDeadArguments(Function &F)
    {
      argTypes.clear();

      for (Argument &Arg : F.args()) {
        // .use_empty() returns true if Arg is dead
        bool argIsDead = Arg.use_empty();
        if (!argIsDead) {
          argTypes.push_back(Arg.getType());
        }
      }
    }

    bool runOnFunction(Function &F) override {

      findDeadArguments(F);

      return false;
    }
  };
}

char DeadArgumentElimination::ID = 0;
static RegisterPass<DeadArgumentElimination> X("our-d-a-elim", "Simple Dead Argument Elimination Pass");