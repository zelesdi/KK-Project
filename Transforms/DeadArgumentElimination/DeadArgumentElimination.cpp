#include <unordered_set>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


namespace {
  struct DeadArgumentElimination : public FunctionPass {
    std::vector<Type *> ArgTypes;
    std::unordered_map<BasicBlock *, BasicBlock *> BasicBlocksMap;

    static char ID;
    DeadArgumentElimination() : FunctionPass(ID) {}

    void findDeadArguments(Function &F)
    {
      for (Argument &Arg : F.args()) {
        // .use_empty() returns true if Arg is dead
        bool ArgIsDead = Arg.use_empty();
        if (!ArgIsDead) {
          ArgTypes.push_back(Arg.getType());
        }
      }
    }

    Function *createNewFunction(Function &F)
    {
      Type *Ty = F.getType();

      FunctionType *NewFuncTy = FunctionType::get(Ty,ArgTypes,false);

      Function *newFunc = Function::Create(
        NewFuncTy,
        Function::ExternalLinkage,   // function can be seen outside the current module
        "new_" + F.getName(),
        F.getParent()
      );

      return newFunc;
    }

    void mapBasicBlocks(Function &F, Function *NewFunc)
    {
      for (BasicBlock &BB : F) {
        BasicBlocksMap[&BB] =
          BasicBlock::Create(F.getContext(),"", NewFunc);
      }
    }

    void cloneFunctionBody(Function &F, Function *NewFunc)
    {
      mapBasicBlocks(F, NewFunc);

      BasicBlock *NewBB;
      for (BasicBlock &BB : F) {
        NewBB = BasicBlocksMap[&BB];

        Instruction *NewInstr;
        for (Instruction &Instr : BB) {
          NewInstr = Instr.clone();
          NewInstr->insertInto(NewBB, NewBB->end());
        }
      }
    }

    bool runOnFunction(Function &F) override {
      ArgTypes.clear();
      BasicBlocksMap.clear();

      findDeadArguments(F);

      // function has no dead arguments
      if (ArgTypes.size() == F.arg_size()) {
        return false;
      }

      Function *NewFunction = createNewFunction(F);
      cloneFunctionBody(F, NewFunction);

      return false;
    }
  };
}

char DeadArgumentElimination::ID = 0;
static RegisterPass<DeadArgumentElimination> X("our-d-a-elim", "Simple Dead Argument Elimination Pass");