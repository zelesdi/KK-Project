#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

struct StrengthReduction : public FunctionPass {
  static char ID;
  StrengthReduction() : FunctionPass(ID) {}

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

  bool runOnFunction(Function &F) override {
    bool Changed = false;

    for (BasicBlock &BB : F) {
      IRBuilder<> Builder(BB.getContext());

      for (Instruction &I : BB) {
        if (BinaryOperator *BinaryOp = dyn_cast<BinaryOperator>(&I)) {
          Value *LeftOperand = BinaryOp->getOperand(0);
          Value *RightOperand = BinaryOp->getOperand(1);

          // --- MULTIPLICATION: a * 2^n → a << n ---
          if (I.getOpcode() == Instruction::Mul) {
            if (!isConstantInt(LeftOperand) && isConstantInt(RightOperand) &&
                isPowerOfTwo(getConstIntValue(RightOperand))) {
              int Power = powerOfTwo(getConstIntValue(RightOperand));
              Instruction *LeftShift = (Instruction *) Builder.CreateShl(LeftOperand, Power);
              LeftShift->insertAfter(&I);
              I.replaceAllUsesWith(LeftShift);
              I.eraseFromParent();
              Changed = true;
            }
            else if (isConstantInt(LeftOperand) && isPowerOfTwo(getConstIntValue(LeftOperand)) &&
                     !isConstantInt(RightOperand)) {
              int Power = powerOfTwo(getConstIntValue(LeftOperand));
              Instruction *LeftShift = (Instruction *) Builder.CreateShl(RightOperand, Power);
              LeftShift->insertAfter(&I);
              I.replaceAllUsesWith(LeftShift);
              I.eraseFromParent();
              Changed = true;
            }
          }

          // --- UNSIGNED DIVISION: a / 2^n → a >> n ---
          if (I.getOpcode() == Instruction::UDiv) {
            if (!isConstantInt(LeftOperand) && isConstantInt(RightOperand) &&
                isPowerOfTwo(getConstUnsignedValue(RightOperand))) {
              int Power = powerOfTwo(getConstIntValue(RightOperand));
              Instruction *RightShift = (Instruction *) Builder.CreateLShr(LeftOperand, Power);
              RightShift->insertAfter(&I);
              I.replaceAllUsesWith(RightShift);
              I.eraseFromParent();
              Changed = true;
            }
          }

          // --- UNSIGNED REMAINDER: a % 2^n → a & (2^n - 1) ---
          if (I.getOpcode() == Instruction::URem) {
            if (!isConstantInt(LeftOperand) && isConstantInt(RightOperand) &&
                isPowerOfTwo(getConstUnsignedValue(RightOperand))) {
              int constant = getConstUnsignedValue(RightOperand);
              Instruction *And = (Instruction *) Builder.CreateAnd(LeftOperand, constant - 1);
              And->insertAfter(&I);
              I.replaceAllUsesWith(And);
              I.eraseFromParent();
              Changed = true;
            }
          }
        }
      }
    }

    return Changed;
  }
};

} // end anonymous namespace

char StrengthReduction::ID = 0;
static RegisterPass<StrengthReduction> X("our-s-r", "Strength Reduction Pass");
