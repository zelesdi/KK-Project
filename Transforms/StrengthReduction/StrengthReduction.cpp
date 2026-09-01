#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>
#include <vector>

using namespace llvm;

namespace {

struct StrengthReduction : public FunctionPass {
  static char ID;
  StrengthReduction() : FunctionPass(ID) {}

  Value *TrueValue;
  Value *FalseValue;

  std::unordered_map<Value *, Value *> VariablesMap;

  std::vector<Instruction *> InstructionsToRemove;
  std::vector<Instruction *> InstructionsToCopy;

  bool isConstantInt(Value *Val) {
    return isa<ConstantInt>(Val);
  }

  int getConstIntValue(Value *Val) {
    ConstantInt *CI = dyn_cast<ConstantInt>(Val);
    return CI->getSExtValue();
  }

  unsigned getConstUnsignedValue(Value *Val) {
    ConstantInt *CI = dyn_cast<ConstantInt>(Val);
    return CI->getZExtValue();
  }

  bool isPowerOfTwo(int x) {
    return x > 0 && (x & (x - 1)) == 0;
  }

  int powerOfTwo(int x) {
    int power = 0;

    while (x > 1) {
      x >>= 1;
      power++;
    }

    return power;
  }

  void mapVariables(Function &F) {
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (isa<LoadInst>(&I)) {
          VariablesMap[&I] = I.getOperand(0);
        }
      }
    }
  }

  BasicBlock *createTrueBasicBlock(Function &F, Value *FunctionParameter) {
    BasicBlock *TrueBasicBlock = BasicBlock::Create(F.getContext(), "", &F, nullptr);
    IRBuilder<> Builder(TrueBasicBlock->getContext());
    Builder.SetInsertPoint(TrueBasicBlock);
    TrueValue = Builder.CreateLoad(Type::getInt32Ty(TrueBasicBlock->getContext()),
      VariablesMap[FunctionParameter]);
    Builder.CreateBr(&F.back());

    return TrueBasicBlock;
  }

  BasicBlock *createFalseBasicBlock(Function &F, Value *FunctionParameter) {
    BasicBlock *FalseBasicBlock = BasicBlock::Create(F.getContext(), "falseBasicBlock",
      &F, nullptr);
    IRBuilder<> Builder(FalseBasicBlock);
    Instruction *Load = Builder.CreateLoad(Type::getInt32Ty(FalseBasicBlock->getContext()),
      VariablesMap[FunctionParameter]);
    FalseValue = Builder.CreateSub(ConstantInt::get(
      Type::getInt32Ty(FalseBasicBlock->getContext()), 0), Load);

    Builder.CreateBr(&F.back());
    return FalseBasicBlock;
  }

  BasicBlock *createMergeBasicBlock(Function &F, Value *VariableToStore,
    BasicBlock *TrueBasicBlock, BasicBlock *FalseBasicBlock) {
    BasicBlock *MergeBasicBlock = BasicBlock::Create(F.getContext(), "", &F, nullptr);
    IRBuilder<> Builder(MergeBasicBlock);

    PHINode *PHI = Builder.CreatePHI(Type::getInt32Ty(MergeBasicBlock->getContext()), 2);
    PHI->addIncoming(TrueValue, TrueBasicBlock);
    PHI->addIncoming(FalseValue, FalseBasicBlock);
    Builder.CreateStore(PHI, VariableToStore);
    Instruction *Ret = Builder.CreateRet(
      ConstantInt::get(Type::getInt32Ty(MergeBasicBlock->getContext()), 0));

    for (int i = 2; i < (int) InstructionsToCopy.size(); i++) {
      Instruction *Copy = InstructionsToCopy[i]->clone();
      Copy->insertBefore(Ret);
      InstructionsToCopy[i]->replaceAllUsesWith(Copy);
    }

    InstructionsToRemove.push_back(Ret);
    return MergeBasicBlock;
  }

  void removeAllInstructions(BasicBlock &BB, Instruction *RemoveFromInstruction) {
    bool Found = false;

    for (Instruction &I : BB) {
      if (&I == RemoveFromInstruction) {
        Found = true;
      }

      if (Found) {
        InstructionsToRemove.push_back(&I);
        InstructionsToCopy.push_back(&I);
      }
    }
  }

  bool runOnFunction(Function &F) override {
    bool Changed = false;

    mapVariables(F);

    for (BasicBlock &BB : F) {
      IRBuilder<> Builder(BB.getContext());

      for (Instruction &I : BB) {
        if (BinaryOperator *BinaryOp = dyn_cast<BinaryOperator>(&I)) {
          Value *LeftOperand = BinaryOp->getOperand(0);
          Value *RightOperand = BinaryOp->getOperand(1);

          if (I.getOpcode() == Instruction::Mul) {
            if (!isConstantInt(LeftOperand) && isConstantInt(RightOperand) &&
              isPowerOfTwo(getConstIntValue(RightOperand))) {
              int Power = powerOfTwo(getConstIntValue(RightOperand));
              Instruction *LeftShift = (Instruction *) Builder.CreateShl(LeftOperand, Power);
              LeftShift->insertAfter(&I);
              I.replaceAllUsesWith(LeftShift);
              InstructionsToRemove.push_back(&I);
              Changed = true;
            }
            else if (isConstantInt(LeftOperand) && isPowerOfTwo(getConstIntValue(LeftOperand)) &&
              !isConstantInt(RightOperand)) {
              int Power = powerOfTwo(getConstIntValue(LeftOperand));
              Instruction *LeftShift = (Instruction *) Builder.CreateShl(RightOperand, Power);
              LeftShift->insertAfter(&I);
              I.replaceAllUsesWith(LeftShift);
              InstructionsToRemove.push_back(&I);
              Changed = true;
            }
          }
          if (I.getOpcode() == Instruction::UDiv) {
            if (!isConstantInt(LeftOperand) && isConstantInt(RightOperand) &&
              isPowerOfTwo(getConstUnsignedValue(RightOperand))) {
              int Power = powerOfTwo(getConstIntValue(RightOperand));
              Instruction *RightShift = (Instruction *) Builder.CreateLShr(LeftOperand, Power);
              RightShift->insertAfter(&I);
              I.replaceAllUsesWith(RightShift);
              InstructionsToRemove.push_back(&I);
              Changed = true;
            }
          }
          if (I.getOpcode() == Instruction::URem) {
            if (!isConstantInt(LeftOperand) && isConstantInt(RightOperand) &&
              isPowerOfTwo(getConstUnsignedValue(RightOperand))) {
              int constant = getConstUnsignedValue(RightOperand);
              Instruction *And = (Instruction *) Builder.CreateAnd(LeftOperand, constant - 1);
              And->insertAfter(&I);
              I.replaceAllUsesWith(And);
              InstructionsToRemove.push_back(&I);
              Changed = true;
            }
          }
        }
        else if (CallInst *Call = dyn_cast<CallInst>(&I)) {
          if (Call->getCalledFunction()->getName().str() == "abs") {
            removeAllInstructions(BB, Call);

            Value *VariableToStore = nullptr;
            if (isa<StoreInst>(Call->getNextNode())) {
              VariableToStore = Call->getNextNode()->getOperand(1);
            }
            else {
              errs() << "Fatal error!\n";
              exit(1);
            }

            Value *FunctionParameter = Call->getOperand(0);

            Instruction *Cmp = (Instruction *) Builder.CreateICmpSGE(FunctionParameter,
              ConstantInt::get(Type::getInt32Ty(BB.getContext()), 0));
            Cmp->insertAfter(Call);

            BasicBlock *TrueBasicBlock = createTrueBasicBlock(F, FunctionParameter);
            BasicBlock *FalseBasicBlock = createFalseBasicBlock(F, FunctionParameter);
            BasicBlock *MergeBasicBlock = createMergeBasicBlock(F, VariableToStore,
              TrueBasicBlock, FalseBasicBlock);

            TrueBasicBlock->getTerminator()->setSuccessor(0, MergeBasicBlock);
            FalseBasicBlock->getTerminator()->setSuccessor(0, MergeBasicBlock);

            Instruction *CondBranch = Builder.CreateCondBr(Cmp, TrueBasicBlock, FalseBasicBlock);
            CondBranch->insertAfter(Cmp);
          }
        }
      }
    }

    for (Instruction *I : InstructionsToRemove) {
      I->eraseFromParent();
    }

    return Changed;
  }
};

} // end anonymous namespace

char StrengthReduction::ID = 0;
static RegisterPass<StrengthReduction> X("our-s-r", "Strength Reduction Pass");
