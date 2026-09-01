#include <vector>
#include <unordered_map>

#include <llvm/IR/Constants.h>

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  // Module pass zato sto menjamo funkciju
  struct DeadArgumentElimination : public ModulePass {
    std::vector<bool> ArgIsDead;
    std::vector<Type *> ArgTypes;
    std::unordered_map<BasicBlock *, BasicBlock *> BasicBlocksMap;
    std::unordered_map<Value *, Value *> VariablesMap;

    static char ID;
    DeadArgumentElimination() : ModulePass(ID) {}

    void findDeadArguments(Function &F)
    {
      ArgIsDead.assign(F.arg_size(), false);
      // Clear ArgTypes from the previous function
      ArgTypes.clear();

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
      // Get return type from the old function
      // int foo(int a) -> getFunctionType() = int (int)
      //                ->   getReturnType() = int
      Type *ReturnTy = OldFunc.getFunctionType()->getReturnType();

      // isVarArg - is function variadic
      FunctionType *NewFuncTy =
          FunctionType::get(ReturnTy, ArgTypes, false);

      Function *NewFunc =
        Function::Create(
          NewFuncTy,
          Function::ExternalLinkage,
          // we can't have two functions with the same name
          "new_" + OldFunc.getName(),
          OldFunc.getParent()
        );

      return NewFunc;
    }

    void mapBasicBlocks(Function &OldFunc, Function &NewFunc)
    {
      for (BasicBlock &BB : OldFunc) {
        BasicBlocksMap[&BB] =
            BasicBlock::Create(
                NewFunc.getContext(),
                "",
                &NewFunc
            );
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
      for (BasicBlock &BB : OldFunc) {
        BasicBlock *NewBB = BasicBlocksMap[&BB];

        for (Instruction &Instr : BB) {
          Instruction *NewInstr = Instr.clone();
          NewInstr->insertInto(NewBB, NewBB->end());

          VariablesMap[&Instr] = NewInstr;
        }
      }

      // update instruction operands
      for (BasicBlock &BB : OldFunc) {
        for (Instruction &Instr : BB) {
          Instruction *NewInstr =
              cast<Instruction>(VariablesMap[&Instr]);

          for (unsigned i = 0; i < NewInstr->getNumOperands(); i++) {
            Value *Operand = NewInstr->getOperand(i);

            // check if we mapped the argument
            auto It = VariablesMap.find(Operand);

            // operand is an argument
            // map old argument into a new one
            if (It != VariablesMap.end()) {
              NewInstr->setOperand(i, It->second);
            }
            // operand is a basic block
            // map old basic block into a new one
            else if (auto *OpBB = dyn_cast<BasicBlock>(Operand)) {
              auto It2 = BasicBlocksMap.find(OpBB);

              if (It2 != BasicBlocksMap.end()) {
                NewInstr->setOperand(i, It2->second);
              }
            }
          }
        }
      }
    }

    void updateOldFunctionCalls(Function &OldFunc, Function &NewFunc)
    {
      // here we will save calls to delete later
      // because we can't delete them while iterating
      std::vector<CallInst *> Calls;

      // save all calls to our old function
      // .users() - list of all users (callers) of a given function
      for (User *U : OldFunc.users()) {
        // check if user is actually a call instruction
        auto *Call = dyn_cast<CallInst>(U);

        if (!Call)
          continue;

        if (Call->getCalledFunction() != &OldFunc)
          continue;

        Calls.push_back(Call);
      }

      // replace all calls of the old function
      // with a new call that contains only live arguments
      for (CallInst *Call : Calls) {
        std::vector<Value *> NewArgs;

        for (unsigned i = 0; i < Call->arg_size(); i++) {
          if (!ArgIsDead[i]) {
            NewArgs.push_back(Call->getArgOperand(i));
          }
        }

        CallInst *NewCall =
          CallInst::Create(
                &NewFunc,
                NewArgs,
                "",
                Call
          );
        // calling convention - how to pass the arguments,
        //                      location of return value, etc...
        NewCall->setCallingConv(Call->getCallingConv());

        Call->replaceAllUsesWith(NewCall);

        Call->eraseFromParent();
      }
    }

    bool processFunction(Function &F)
    {
      if (F.isDeclaration()) {
        return false;
      }
      if (F.arg_empty()) {
        return false;
      }

      findDeadArguments(F);

      if (ArgTypes.size() == F.arg_size()) {
        return false;
      }

      Function *NewFunc = createNewFunction(F);

      cloneFunctionBody(F, *NewFunc);
      updateOldFunctionCalls(F, *NewFunc);
      NewFunc->takeName(&F);
      F.eraseFromParent();

      return true;
    }

    bool runOnModule(Module &M) override
    {
      bool Changed = false;

      /*
       * Save all functions first because processFunction()
       * deletes functions from the module.
       */
      // save all functions from the module
      std::vector<Function *> Functions;

      for (Function &F : M) {
        Functions.push_back(&F);
      }

      for (Function *F : Functions) {
        if (F->getParent() != &M) {
          continue;
        }

        Changed |= processFunction(*F);
      }

      return Changed;
    }
  };
}

char DeadArgumentElimination::ID = 0;

static RegisterPass<DeadArgumentElimination>
    X(
        "simple-dae",
        "Simple Dead Argument Elimination Pass"
    );
