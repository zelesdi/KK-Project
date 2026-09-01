#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace
{

struct StrengthReduction : public FunctionPass {
    static char ID;

    StrengthReduction() : FunctionPass(ID)
    {}

    bool isConstantInt(Value* Val)
    {
        return isa<ConstantInt>(Val);
    }

    int getConstIntValue(Value* Val)
    {
        ConstantInt* CI = dyn_cast<ConstantInt>(Val);
        return CI->getSExtValue();
    }

    unsigned getConstUnsignedValue(Value* Val)
    {
        ConstantInt* CI = dyn_cast<ConstantInt>(Val);
        return CI->getZExtValue();
    }

    bool isPowerOfTwo(int x)
    {
        return x > 0 && (x & (x - 1)) == 0;
    }

    int powerOfTwo(int x)
    {
        int power = 0;
        while(x > 1) {
            x >>= 1;
            power++;
        }
        return power;
    }

    bool runOnFunction(Function& F) override
    {
        bool Changed = false;

        for(BasicBlock& BB : F) {
            for(auto& I : llvm::make_early_inc_range(BB)) {
                auto* BinaryOp = dyn_cast<BinaryOperator>(&I);
                if(!BinaryOp)
                    continue;

                IRBuilder<> Builder(&I);

                Value* LHS = BinaryOp->getOperand(0);
                Value* RHS = BinaryOp->getOperand(1);

                // --- mul ---
                if(I.getOpcode() == Instruction::Mul) {
                    // a * 2^n -> a << n
                    if(!isConstantInt(LHS) &&
                        isConstantInt(RHS) &&
                        isPowerOfTwo(getConstIntValue(RHS))) {
                        int Power = powerOfTwo(getConstIntValue(RHS));
                        Value* Shift = Builder.CreateShl(LHS, Power);
                        I.replaceAllUsesWith(Shift);
                        I.eraseFromParent();
                        Changed = true;
                        continue;
                    }

                    // 2^n * a -> a << n
                    if(isConstantInt(LHS) &&
                        !isConstantInt(RHS) &&
                        isPowerOfTwo(getConstIntValue(LHS))) {
                        int Power = powerOfTwo(getConstIntValue(LHS));
                        Value* Shift = Builder.CreateShl(RHS, Power);
                        I.replaceAllUsesWith(Shift);
                        I.eraseFromParent();
                        Changed = true;
                        continue;
                    }

                    // a * (2^n + 1) -> (a << n) + a
                    if(!isConstantInt(LHS) &&
                        isConstantInt(RHS) &&
                        isPowerOfTwo(getConstIntValue(RHS) - 1)) {
                        int n = powerOfTwo(getConstIntValue(RHS) - 1);
                        Value* Shift = Builder.CreateShl(LHS, n);
                        Value* Add = Builder.CreateAdd(Shift, LHS);
                        I.replaceAllUsesWith(Add);
                        I.eraseFromParent();
                        Changed = true;
                        continue;
                    }

                    // (2^n + 1) * a -> (a << n) + a
                    if(isConstantInt(LHS) &&
                        !isConstantInt(RHS) &&
                        isPowerOfTwo(getConstIntValue(LHS) - 1)) {
                        int n = powerOfTwo(getConstIntValue(LHS) - 1);
                        Value* Shift = Builder.CreateShl(RHS, n);
                        Value* Add = Builder.CreateAdd(Shift, RHS);
                        I.replaceAllUsesWith(Add);
                        I.eraseFromParent();
                        Changed = true;
                        continue;
                    }

                    // a * (2^n - 1) -> (a << n) - a
                    if(!isConstantInt(LHS) &&
                        isConstantInt(RHS) &&
                        isPowerOfTwo(getConstIntValue(RHS) + 1)) {
                        int n = powerOfTwo(getConstIntValue(RHS) + 1);
                        Value* Shift = Builder.CreateShl(LHS, n);
                        Value* Sub = Builder.CreateSub(Shift, LHS);
                        I.replaceAllUsesWith(Sub);
                        I.eraseFromParent();
                        Changed = true;
                        continue;
                    }

                    // (2^n - 1) * a -> (a << n) - a
                    if(isConstantInt(LHS) &&
                        !isConstantInt(RHS) &&
                        isPowerOfTwo(getConstIntValue(LHS) + 1)) {
                        int n = powerOfTwo(getConstIntValue(LHS) + 1);
                        Value* Shift = Builder.CreateShl(RHS, n);
                        Value* Sub = Builder.CreateSub(Shift, RHS);
                        I.replaceAllUsesWith(Sub);
                        I.eraseFromParent();
                        Changed = true;
                        continue;
                    }
                    continue;
                }

                // --- udiv ---
                if(I.getOpcode() == Instruction::UDiv) {
                    // a / 2^n -> a >> n
                    if(!isConstantInt(LHS) &&
                        isConstantInt(RHS) &&
                        isPowerOfTwo(getConstUnsignedValue(RHS))) {
                        int Power = powerOfTwo(getConstIntValue(RHS));
                        Value* Shift = Builder.CreateLShr(LHS, Power);
                        I.replaceAllUsesWith(Shift);
                        I.eraseFromParent();
                        Changed = true;
                    }
                    continue;
                }

                // --- urem ---
                if(I.getOpcode() == Instruction::URem) {
                    // a % 2^n -> a & (2^n - 1)
                    if(!isConstantInt(LHS) &&
                        isConstantInt(RHS) &&
                        isPowerOfTwo(getConstUnsignedValue(RHS))) {
                        int constant = getConstUnsignedValue(RHS);
                        Value* And = Builder.CreateAnd(LHS, constant - 1);
                        I.replaceAllUsesWith(And);
                        I.eraseFromParent();
                        Changed = true;
                    }
                    continue;
                }
            }
        }

        return Changed;
    }
};

char StrengthReduction::ID = 0;
static RegisterPass<StrengthReduction> X("our-s-r", "Strength Reduction Pass");

}  // end anonymous namespace
