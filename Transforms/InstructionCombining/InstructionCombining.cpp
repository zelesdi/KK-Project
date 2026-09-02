#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"

#include <vector>
#include <unordered_set>

using namespace llvm;


namespace {
  struct InstructionCombining : public FunctionPass {
    static char ID;
    InstructionCombining() : FunctionPass(ID) {}

    std::unordered_set<Instruction*> Erase;

    //Rule 1: Operand canonicalization
    // C op X  ->  X op C     (commutative ops only: add, mul, and, or, xor)
    bool OpCanonicalization(Instruction *I)
    {
      auto *BinOp = dyn_cast<BinaryOperator>(I);
      if (!BinOp)
        return false;

      if (!BinOp->isCommutative())
        return false;

      Value *LOperand = BinOp->getOperand(0);
      Value *ROperand = BinOp->getOperand(1);

      if (isa<Constant>(LOperand) && !isa<Constant>(ROperand)) {
        BinOp->setOperand(0, ROperand);
        BinOp->setOperand(1, LOperand);
        return true;
      }

      return false;

    }

    //Rule 2: Sub to add canonicalization
    // X - C  ->  X + (-C)

    Value *SubToAddCanonicalization(Instruction *I) {
      auto *BinOp = dyn_cast<BinaryOperator>(I);
      if (!BinOp || BinOp->getOpcode() != Instruction::Sub)
        return nullptr;

      auto *Const = dyn_cast<ConstantInt>(BinOp->getOperand(1));
      if (!Const)
        return nullptr; // only handle X - C, not C - X

      Value *X = BinOp->getOperand(0);

      IRBuilder<> Builder(BinOp);
      Value *NegConst = Builder.CreateNeg(Const); // folds to a constant at compile time
      return Builder.CreateAdd(X, NegConst);
    }

    //Rule 3: Multiply-to-Shift Strength Reduction
    // X * C  ->  X << log2(C)      (C is a positive power of two)

    Value *MultiplyToShiftStrengthreduction(Instruction *I) {
      auto *BinOp = dyn_cast<BinaryOperator>(I);
      if (!BinOp || BinOp->getOpcode() != Instruction::Mul)
        return nullptr;

      auto *Const = dyn_cast<ConstantInt>(BinOp->getOperand(1));
      if (!Const)
        return nullptr;

      const APInt &Val = Const->getValue();
      if (Val.isNegative() || !Val.isPowerOf2())
        return nullptr;

      IRBuilder<> Builder(BinOp);
      return Builder.CreateShl(BinOp->getOperand(0), ConstantInt::get(BinOp->getType(), Val.logBase2()));
    }

    //Rule 4: Nested Shift Folding
    //shl(shl(X, C1), C2)  ->  shl(X, C1 + C2)

    Value *NestedShiftFolding(Instruction *I) {
      auto *BinOp = dyn_cast<BinaryOperator>(I);
      if (!BinOp || BinOp->getOpcode() != Instruction::Shl)
        return nullptr;

      auto *Const2 = dyn_cast<ConstantInt>(BinOp->getOperand(1));
      if (!Const2)
        return nullptr;

      auto *InnerBinOp = dyn_cast<BinaryOperator>(BinOp->getOperand(0));
      if (!InnerBinOp || InnerBinOp->getOpcode() != Instruction::Shl)
        return nullptr;

      auto *Const1 = dyn_cast<ConstantInt>(InnerBinOp->getOperand(1));
      if (!Const1)
        return nullptr;

      if (!InnerBinOp->hasOneUse())
        return nullptr;

      Value *X = InnerBinOp->getOperand(0);
      Type *Ty = BinOp->getType();
      unsigned BitWidth = Ty->getScalarSizeInBits();

      APInt V1 = Const1->getValue().zext(BitWidth + 1);
      APInt V2 = Const2->getValue().zext(BitWidth + 1);
      APInt Sum = V1 + V2;

      if (Sum.uge(BitWidth))
      {
        return ConstantInt::get(Ty, 0);
      }

      IRBuilder<> Builder(BinOp);
      Value *SumShift = Builder.CreateAdd(Const1, Const2); // folds at compile time
      return Builder.CreateShl(X, SumShift);
    }

    //Rule 5: Shift bitwise reassociation
    //shl(or/and/xor(X, C1), C2)  ->  or/and/xor(shl(X, C2), C1 shl C2)
    //Sinks the shift down next to X, ahead of the bitwise op, matching the
    // canonical ordering: 1. shl -> 2. or -> 3. and -> 4. xor.

    Value *ShiftBitwiseReassociation(Instruction *I) {
      auto *BinOp = dyn_cast<BinaryOperator>(I);
      if (!BinOp   || BinOp->getOpcode() != Instruction::Shl)
        return nullptr;

      auto *ShiftAmt = dyn_cast<ConstantInt>(BinOp->getOperand(1));
      if (!ShiftAmt)
        return nullptr; // shift by a non-constant amount - leave alone

      auto *InnerBinOp = dyn_cast<BinaryOperator>(BinOp->getOperand(0));
      if (!InnerBinOp)
        return nullptr;

      unsigned InnerOp = InnerBinOp->getOpcode();
      if (InnerOp != Instruction::Or && InnerOp != Instruction::And &&
          InnerOp != Instruction::Xor)
        return nullptr;

      auto *InnerOpConst = dyn_cast<ConstantInt>(InnerBinOp->getOperand(1));
      if (!InnerOpConst)
        return nullptr; // inner operand isn't of the form (X, constant)

      if (!InnerBinOp->hasOneUse())
        return nullptr; // only safe if Inner has no other uses besides this shl

      Value *X = InnerBinOp->getOperand(0);

      IRBuilder<> Builder(BinOp);
      Value *NewShift = Builder.CreateShl(X, ShiftAmt);
      Value *NewConst = Builder.CreateShl(InnerOpConst, ShiftAmt); // folds at compile time

      switch (InnerOp) {
      case Instruction::Or:
        return Builder.CreateOr(NewShift, NewConst);
      case Instruction::And:
        return Builder.CreateAnd(NewShift, NewConst);
      case Instruction::Xor:
        return Builder.CreateXor(NewShift, NewConst);
      default:
        llvm_unreachable("unexpected inner opcode");
      }
    }

    //Rule 6: Constant Add Reassociation
    //(X + C1) + C2  ->  X + (C1 + C2)

    Value *ConstantAddReassociation(Instruction *I) {
      auto *BinOp = dyn_cast<BinaryOperator>(I);
      if (!BinOp || BinOp->getOpcode() != Instruction::Add)
        return nullptr;

      auto *Const2 = dyn_cast<ConstantInt>(BinOp->getOperand(1));
      if (!Const2)
        return nullptr; // outer RHS must be a constant

      auto *InnerBinOp = dyn_cast<BinaryOperator>(BinOp->getOperand(0));
      if (!InnerBinOp || InnerBinOp->getOpcode() != Instruction::Add)
        return nullptr;

      auto *Const1 = dyn_cast<ConstantInt>(InnerBinOp->getOperand(1));
      if (!Const1)
        return nullptr; // inner RHS must also be a constant

      if (!InnerBinOp->hasOneUse())
        return nullptr; // don't fold away an inner add still used elsewhere

      Value *X = InnerBinOp->getOperand(0);

      IRBuilder<> Builder(BinOp);
      Value *FoldedConst = Builder.CreateAdd(Const1, Const2); // folds at compile time
      return Builder.CreateAdd(X, FoldedConst);
    }

    bool visit(Instruction *I,Value *&ReplacemantOut)
    {
      ReplacemantOut = nullptr;

      if (OpCanonicalization(I))
      {
        return true;
      }
      if (Value *r = SubToAddCanonicalization(I))
      {
        ReplacemantOut = r;
        return true;
      }
      if (Value *r = MultiplyToShiftStrengthreduction(I))
      {
        ReplacemantOut = r;
        return true;
      }
      if (Value *r = NestedShiftFolding(I))
      {
        ReplacemantOut = r;
        return true;
      }
      if (Value *r = ShiftBitwiseReassociation(I))
      {
        ReplacemantOut = r;
        return true;
      }
      if (Value *r = ConstantAddReassociation(I))
      {
        ReplacemantOut = r;
        return true;
      }
      return false;
    }

    void addInstructions(Instruction *I, std::vector<Instruction*> &List)
    {
      for (User *U : I->users())
      {
        if (auto *UI = dyn_cast<Instruction>(U))
        {
          List.push_back(UI);
        }
      }
    }

    bool runOnFunction(Function &F) override {
      bool Changed = false;
      Erase.clear();

      std::vector<Instruction *> List;
      for (Instruction &I : instructions(F))
        List.push_back(&I);

      while (!List.empty()) {
        Instruction *I = List.back();
        List.pop_back();

        if (Erase.find(I) != Erase.end())
          continue; // already dead, skip

        Value *Replacement = nullptr;
        if (!visit(I, Replacement))
          continue; // no rule matched

        Changed = true;

        I->replaceAllUsesWith(Replacement);
        Erase.insert(I);

        if (auto *RI = dyn_cast<Instruction>(Replacement)) {
          List.push_back(RI);
          addInstructions(RI, List);
        }
      }

      for (Instruction *I : Erase)
        I->eraseFromParent();

      return Changed;

    }
  };
}

char InstructionCombining::ID = 3;
static RegisterPass<InstructionCombining> X("instruction-combining", "Simple Instruction Combining Pass"
" combines instructions to make simpler instructions", false, false);