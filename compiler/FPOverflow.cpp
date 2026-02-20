#include "FPOverflow.h"

using namespace llvm;

void FPOverflowChecker::visitBinaryOperator(BinaryOperator &I) {
  if (!I.getType()->isFloatingPointTy())
    return;

  Value *overflowCond = getOverflowCond(I);

  IRBuilder<> IRB(&I);

  if (I.getOpcode() == Instruction::FDiv) {
    Value *devidedByZeroCond = getDevidedByZeroCondition(I);
    overflowCond = IRB.CreateOr(overflowCond, devidedByZeroCond);
  }

  // ==============================
  // control flow split to handle overflow
  // ==============================

  BasicBlock *CurBB = I.getParent();
  // Function *F = CurBB->getParent();

  // split current basic block
  BasicBlock *ContBB = CurBB->splitBasicBlock(I.getIterator(), "cont");

  // remove the unconditional branch added by splitBasicBlock
  CurBB->getTerminator()->eraseFromParent();

  IRB.SetInsertPoint(CurBB);

  // both goto contBB
  IRB.CreateCondBr(overflowCond, ContBB, ContBB);
}

Value *FPOverflowChecker::getOverflowCond(llvm::BinaryOperator &I) {

  IRBuilder<> IRB(&I);
  Value *Result = &I;

  Value *PosInf = ConstantFP::getInfinity(Result->getType());
  Value *NegInf = ConstantFP::getInfinity(Result->getType(), true);

  Value *IsPosInf = IRB.CreateFCmpOEQ(Result, PosInf);
  Value *IsNegInf = IRB.CreateFCmpOEQ(Result, NegInf);

  return IRB.CreateOr(IsPosInf, IsNegInf);
}

Value *FPOverflowChecker::getDevidedByZeroCondition(llvm::BinaryOperator &I) {
  Value *Divisor = I.getOperand(1);

  IRBuilder<> IRB(&I);

  Value *Zero = ConstantFP::get(Divisor->getType(), 0.0);

  return IRB.CreateFCmpOEQ(Divisor, Zero);
}