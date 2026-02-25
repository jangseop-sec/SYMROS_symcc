#include "FPOverflow.h"

using namespace llvm;

void FPOverflowChecker::visitBinaryOperator(BinaryOperator &I) {
  if (!I.getType()->isFloatingPointTy())
    return;

  // TODO implement!!!
  LLVMContext &Ctx = I.getContext();
  Function *F = I.getFunction();
  BasicBlock *OriginBB = I.getParent();

  Instruction *NextInst = I.getNextNode();
  if (!NextInst)
    return;

  BasicBlock *ContBB = OriginBB->splitBasicBlock(NextInst, "origin_cont");

  OriginBB->getTerminator()->eraseFromParent();

  // new basic blocks for numeric execption check
  BasicBlock *OverflowCheckBB = BasicBlock::Create(Ctx, "overflow_check", F);
  BasicBlock *DividedByZeroCheckBB =
      BasicBlock::Create(Ctx, "devided_by_zero_check", F);
  BasicBlock *BoundCheckBB = BasicBlock::Create(Ctx, "bound_check", F);

  // connect origin branch to overflow check branch
  IRBuilder<> IRB(OriginBB);
  IRB.CreateBr(OverflowCheckBB);

  // overflow check branch
  IRBuilder<> IR1(OverflowCheckBB);

  Value *OverflowCond = getOverflowCond(I, IR1);

  BranchInst *OverflowCheckBranch =
      IR1.CreateCondBr(OverflowCond, BoundCheckBB, BoundCheckBB);

  // set metadata
  OverflowCheckBranch->setMetadata(
      "symros.check", MDNode::get(Ctx, MDString::get(Ctx, "fp_overflow")));
  OverflowCheckBranch->setDebugLoc(I.getDebugLoc());

  // bound check branch
  IRBuilder<> IR2(BoundCheckBB);

  Value *BoundCond = getSementicBoundCondition(I, IR2);
  BasicBlock *NextBB = nullptr;

  if (I.getOpcode() == Instruction::FDiv) {
    NextBB = DividedByZeroCheckBB;
  } else {
    NextBB = ContBB;
  }

  BranchInst *BoundCheckBranch = IR2.CreateCondBr(BoundCond, NextBB, NextBB);

  // set metadata
  BoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "fp_exceptional_value")));
  BoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // divided by zero check (optional)
  if (I.getOpcode() == Instruction::FDiv) {
    IRBuilder<> IR3(DividedByZeroCheckBB);

    Value *DividedByZeroCond = getDividedByZeroCondition(I, IR3);

    BranchInst *DividedByZeroCheckBranch =
        IR3.CreateCondBr(DividedByZeroCond, ContBB, ContBB);

    // set metadata
    DividedByZeroCheckBranch->setMetadata(
        "symros.check",
        MDNode::get(Ctx, MDString::get(Ctx, "fp_divided_by_zero")));
    DividedByZeroCheckBranch->setDebugLoc(I.getDebugLoc());
  }
}

Value *FPOverflowChecker::getOverflowCond(llvm::BinaryOperator &I,
                                          llvm::IRBuilder<> &IRB) {

  Value *Result = &I;

  Value *PosInf = ConstantFP::getInfinity(Result->getType());
  Value *NegInf = ConstantFP::getInfinity(Result->getType(), true);

  Value *IsPosInf = IRB.CreateFCmpOEQ(Result, PosInf);
  Value *IsNegInf = IRB.CreateFCmpOEQ(Result, NegInf);

  return IRB.CreateOr(IsPosInf, IsNegInf);
}

Value *FPOverflowChecker::getDividedByZeroCondition(llvm::BinaryOperator &I,
                                                    llvm::IRBuilder<> &IRB) {
  Value *Divisor = I.getOperand(1);

  Value *Zero = ConstantFP::get(Divisor->getType(), 0.0);

  return IRB.CreateFCmpOEQ(Divisor, Zero);
}

Value *FPOverflowChecker::getSementicBoundCondition(llvm::BinaryOperator &I,
                                                    llvm::IRBuilder<> &IRB) {

  Value *Result = &I;

  Value *LowerBound = IRB.CreateFCmpOLT(
      Result, ConstantFP::get(Result->getType(), -sementic_threshold));
  Value *UpperBound = IRB.CreateFCmpOGT(
      Result, ConstantFP::get(Result->getType(), sementic_threshold));

  return IRB.CreateOr(LowerBound, UpperBound);
}

void FPOverflowChecker::setSementicThreshold(double sementic_threshold) {
  this->sementic_threshold = sementic_threshold;
}