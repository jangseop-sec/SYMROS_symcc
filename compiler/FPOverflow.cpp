#include "FPOverflow.h"

using namespace llvm;

void FPOverflowChecker::visitBinaryOperator(BinaryOperator &I) {
  if (!I.getType()->isFloatingPointTy())
    return;

  Value *overflowCond = getOverflowCond(I);

  IRBuilder<> IRB(&I);

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
  BranchInst *CreatedBr = IRB.CreateCondBr(overflowCond, ContBB, ContBB);

  // set metadata to identify
  LLVMContext &Ctx = I.getContext();
  MDNode *Tag = MDNode::get(Ctx, MDString::get(Ctx, "fp_overflow"));

  CreatedBr->setMetadata("symros.check", Tag);

  if (I.getOpcode() == Instruction::FDiv) {

    Value *dividedByZeroCond = getDividedByZeroCondition(I);

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
    BranchInst *CreatedBr = IRB.CreateCondBr(dividedByZeroCond, ContBB, ContBB);

    // set metadata to identify
    LLVMContext &Ctx = I.getContext();
    MDNode *Tag = MDNode::get(Ctx, MDString::get(Ctx, "fp_devided_by_zero"));

    CreatedBr->setMetadata("symros.check", Tag);
  }
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

Value *FPOverflowChecker::getDividedByZeroCondition(llvm::BinaryOperator &I) {
  Value *Divisor = I.getOperand(1);

  IRBuilder<> IRB(&I);

  Value *Zero = ConstantFP::get(Divisor->getType(), 0.0);

  return IRB.CreateFCmpOEQ(Divisor, Zero);
}