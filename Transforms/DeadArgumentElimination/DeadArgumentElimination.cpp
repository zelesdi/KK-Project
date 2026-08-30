#include <llvm/IR/Constants.h>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


namespace {
  struct DeadArgumentElimination : public FunctionPass {
    std::vector<bool> ArgIsDead;
    std::vector<Type *> ArgTypes;
    std::unordered_map<BasicBlock *, BasicBlock *> BasicBlocksMap;
    std::unordered_map<Value *, Value *> VariablesMap;

    static char ID;
    DeadArgumentElimination() : FunctionPass(ID) {}

    void findDeadArguments(Function &F)
    {
      ArgIsDead.assign(F.arg_size(), false);
      size_t i = 0;
      for (Argument &Arg : F.args()) {
        // .use_empty() returns true if Arg is dead
        if (!(ArgIsDead[i] = Arg.use_empty())) {
          ArgTypes.push_back(Arg.getType());
        }
        i++;
      }
    }

    Function *createNewFunction(Function &OldFunc)
    {
      Type *Ty = OldFunc.getFunctionType()->getReturnType();

      FunctionType *NewFuncTy = FunctionType::get(Ty,ArgTypes,false);

      Function *newFunc = Function::Create(
        NewFuncTy,
        Function::ExternalLinkage,   // function can be seen outside the current module
        "new_" + OldFunc.getName(),
        OldFunc.getParent()
      );

      return newFunc;
    }

    void mapBasicBlocks(Function &OldFunc, Function &NewFunc)
    {
      for (BasicBlock &BB : OldFunc) {
        BasicBlocksMap[&BB] =
          BasicBlock::Create(OldFunc.getContext(),"", &NewFunc);
      }
    }

    void mapVariables(Function &OldFunc, Function &NewFunc)
    {
      Argument *NewArg = NewFunc.arg_begin();
      size_t i = 0;
      for (Argument &OldArg : OldFunc.args()) {
        if (!ArgIsDead[i]) {
          VariablesMap[&OldArg] = NewArg;
          NewArg++;
        }
        i++;
      }
    }

    void cloneFunctionBody(Function &OldFunc, Function &NewFunc)
    {
      mapBasicBlocks(OldFunc, NewFunc);
      mapVariables(OldFunc, NewFunc);

      // clone instructions from old basic blocks into new ones
      // map instructions
      for (BasicBlock &BB : OldFunc) {
        BasicBlock *NewBB = BasicBlocksMap[&BB];
        for (Instruction &Instr : BB) {
          Instruction *NewInstr = Instr.clone();
          NewInstr->insertInto(NewBB, NewBB->end());
          VariablesMap[&Instr] = NewInstr;
        }
      }

      for (BasicBlock &BB : OldFunc) {
        for (Instruction &Instr : BB) {
          Instruction *NewInstr = cast<Instruction>(VariablesMap[&Instr]);
          for (unsigned i = 0; i < NewInstr->getNumOperands(); i++) {
            Value *Operand = NewInstr->getOperand(i);

            auto It = VariablesMap.find(Operand);
            if (It != VariablesMap.end()) {
              NewInstr->setOperand(i, It->second);
            }
            else if (auto *OpBB = dyn_cast<BasicBlock>(Operand)) {
              auto It2 = BasicBlocksMap.find(OpBB);
              if (It2 != BasicBlocksMap.end()) {
                NewInstr->setOperand(i, It2->second);
              }
            }
            // TODO: PHI Nodes
          }
        }
      }
    }

    void updateOldFunctionCalls(Function &OldFunc, Function &NewFunc)
    {
      for (User *U : llvm::make_early_inc_range(OldFunc.users())) {
        auto *Call = dyn_cast<CallInst>(U);
        if (!Call || Call->getCalledFunction() != &OldFunc) {
          continue;
        }

        std::vector<Value *> NewArgs;
        for (unsigned i = 0; i < Call->arg_size(); i++) {
          if (!ArgIsDead[i]) {
            NewArgs.push_back(Call->getArgOperand(i));
          }
        }

          CallInst *NewCall = CallInst::Create(&NewFunc, NewArgs, "", Call);
          NewCall->setCallingConv(Call->getCallingConv());
          Call->replaceAllUsesWith(NewCall);
          Call->eraseFromParent();
        }
    }

    void deleteOldFunctionCalls(Function &OldFunc, Function &NewFunc)
    {
      NewFunc.takeName(&OldFunc);
      OldFunc.replaceAllUsesWith(UndefValue::get(OldFunc.getType()));
      OldFunc.eraseFromParent();
    }

    bool runOnFunction(Function &F) override {
      ArgTypes.clear();
      BasicBlocksMap.clear();
      VariablesMap.clear();

      findDeadArguments(F);

      // function has no dead arguments
      if (ArgTypes.size() == F.arg_size()) {
        return false;
      }

      Function *NewFunc = createNewFunction(F);
      cloneFunctionBody(F, *NewFunc);
      updateOldFunctionCalls(F, *NewFunc);
      deleteOldFunctionCalls(F, *NewFunc);

      return true;
    }
  };
}

char DeadArgumentElimination::ID = 0;
static RegisterPass<DeadArgumentElimination> X("simple-dae", "Simple Dead Argument Elimination Pass");