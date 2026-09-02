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

    bool isSumOfTwoPowers(int x, int& p, int& q)
    {
        if(x <= 0)
            return false;
        int bits = 0;
        for(int i = 0; i < 32; i++) {
            if(x & (1 << i)) {
                if(bits == 0)
                    p = i;
                else if(bits == 1)
                    q = i;
                bits++;
            }
            if(bits > 2)
                return false;
        }
        return bits == 2;
    }

    bool isDifferenceOfTwoPowers(int x, int& p, int& q)
    {
        if(x <= 0)
            return false;
        int next = 1;
        p = 0;
        while(next <= x) {
            next <<= 1;
            p++;
        }
        int diff = next - x;
        if(diff <= 0 || !isPowerOfTwo(diff))
            return false;
        q = 0;
        int temp = diff;
        while(temp > 1) {
            temp >>= 1;
            q++;
        }
        return true;
    }

    Value* optimizeMultiplication(Value* Var, int Const, IRBuilder<>& Builder)
    {
        if(Const == 0)
            return ConstantInt::get(Var->getType(), 0);
        if(Const == 1)
            return Var;
        if(Const == -1)
            return Builder.CreateNeg(Var);

        int posConst = (Const < 0) ? -Const : Const;

        // a * 2^n -> a << n
        if(isPowerOfTwo(posConst)) {
            int Power = powerOfTwo(posConst);
            Value* Result = Builder.CreateShl(Var, Power);
            return (Const < 0) ? Builder.CreateNeg(Result) : Result;
        }

        // 2^n + 1: a * (2^n + 1) -> (a << n) + a
        if(isPowerOfTwo(posConst - 1)) {
            int n = powerOfTwo(posConst - 1);
            Value* Shift = Builder.CreateShl(Var, n);
            Value* Result = Builder.CreateAdd(Shift, Var);
            return (Const < 0) ? Builder.CreateNeg(Result) : Result;
        }

        // 2^n - 1: a * (2^n - 1) -> (a << n) - a
        if(isPowerOfTwo(posConst + 1)) {
            int n = powerOfTwo(posConst + 1);
            Value* Shift = Builder.CreateShl(Var, n);
            Value* Result = Builder.CreateSub(Shift, Var);
            return (Const < 0) ? Builder.CreateNeg(Result) : Result;
        }

        // a * (2^p + 2^q) -> (a << p) + (a << q)
        int p, q;
        if(isSumOfTwoPowers(posConst, p, q)) {
            Value* Shift1 = Builder.CreateShl(Var, p);
            Value* Shift2 = Builder.CreateShl(Var, q);
            Value* Result = Builder.CreateAdd(Shift1, Shift2);
            return (Const < 0) ? Builder.CreateNeg(Result) : Result;
        }

        // a * (2^p - 2^q) -> (a << p) - (a << q)
        if(isDifferenceOfTwoPowers(posConst, p, q)) {
            Value* Shift1 = Builder.CreateShl(Var, p);
            Value* Shift2 = Builder.CreateShl(Var, q);
            Value* Result = Builder.CreateSub(Shift1, Shift2);
            return (Const < 0) ? Builder.CreateNeg(Result) : Result;
        }

        return nullptr;
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
                    // x * const
                    if(!isConstantInt(LHS) && isConstantInt(RHS)) {
                        int ConstVal = getConstIntValue(RHS);
                        Value* Result = optimizeMultiplication(LHS, ConstVal, Builder);
                        if(Result) {
                            I.replaceAllUsesWith(Result);
                            I.eraseFromParent();
                            Changed = true;
                            continue;
                        }
                    }

                    // const * x
                    if(isConstantInt(LHS) && !isConstantInt(RHS)) {
                        int ConstVal = getConstIntValue(LHS);
                        Value* Result = optimizeMultiplication(RHS, ConstVal, Builder);
                        if(Result) {
                            I.replaceAllUsesWith(Result);
                            I.eraseFromParent();
                            Changed = true;
                            continue;
                        }
                    }

                    continue;
                }

                // --- udiv ---
                if(I.getOpcode() == Instruction::UDiv) {
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
static RegisterPass<StrengthReduction> X("simple-sr", "Strength Reduction Pass");

}  // end anonymous namespace
