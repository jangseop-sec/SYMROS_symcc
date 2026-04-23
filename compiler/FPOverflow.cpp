#include "FPOverflow.h"

using namespace llvm;

void FPOverflowChecker::visitBinaryOperator(BinaryOperator &I) {
  // result check
  if (!I.getType()->isFloatingPointTy())
    return;

  // operand check
  if (!I.getOperand(0)->getType()->isFloatingPointTy() ||
      !I.getOperand(1)->getType()->isFloatingPointTy())
    return;

  // opcode filter
  if (I.getOpcode() != Instruction::FAdd &&
      I.getOpcode() != Instruction::FSub &&
      I.getOpcode() != Instruction::FMul &&
      I.getOpcode() != Instruction::FDiv &&
      I.getOpcode() != Instruction::FRem)
    return;

  // nnan, ninf -> compiler assumption 
  if (I.hasNoNaNs() && I.hasNoInfs())
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
  BasicBlock *MinusBoundCheckBB = BasicBlock::Create(Ctx, "minus_bound_check", F);
  BasicBlock *MaxBoundCheckBB = BasicBlock::Create(Ctx, "max_bound_check", F);
  BasicBlock *MinBoundCheckBB = BasicBlock::Create(Ctx, "min_bound_check", F);

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

  BranchInst *BoundCheckBranch = IR2.CreateCondBr(BoundCond, DividedByZeroCheckBB, DividedByZeroCheckBB);

  // set metadata
  BoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "fp_exceptional_value")));
  BoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // divided by zero check branch
  // if (I.getOpcode() == Instruction::FDiv) {
  IRBuilder<> IR3(DividedByZeroCheckBB);
  bool is_div = true;

  if (I.getOpcode() != Instruction::FDiv) {
    is_div = false;
  }

  Value *DividedByZeroCond = getDividedByZeroCondition(I, IR3, is_div);

  BranchInst *DividedByZeroCheckBranch =
      IR3.CreateCondBr(DividedByZeroCond, MinusBoundCheckBB, MinBoundCheckBB);

  // set metadata
  DividedByZeroCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "fp_divided_by_zero")));
  DividedByZeroCheckBranch->setDebugLoc(I.getDebugLoc());
  
  // sementic minus bound check branch
  IRBuilder<> IR4(MinusBoundCheckBB);
  Value *MinusBoundCond = getSementicMinusBoundCondition(I, IR4);
  BranchInst *MinusBoundCheckBranch =
      IR4.CreateCondBr(MinusBoundCond, MaxBoundCheckBB, MaxBoundCheckBB);
  
  // set metadata
  MinusBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "fp_exceptional_minus_value")));
  MinusBoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // max bound check branch
  IRBuilder<> IR5(MaxBoundCheckBB);
  Value *MaxBoundCond = getSemnticMaxBoundCondition(I, IR5);
  BranchInst *MaxBoundCheckBranch =
      IR5.CreateCondBr(MaxBoundCond, MinBoundCheckBB, MinBoundCheckBB);
  
  // set metadata
  MaxBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "fp_exceptional_max_value")));
  MaxBoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // min bound check branch
  IRBuilder<> IR6(MinBoundCheckBB);
  Value *MinBoundCond = getSementicMinBoundCondition(I, IR6);
  BranchInst *MinBoundCheckBranch =
      IR6.CreateCondBr(MinBoundCond, ContBB, ContBB);
  
  // set metadata
  MinBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "fp_exceptional_min_value")));
  MinBoundCheckBranch->setDebugLoc(I.getDebugLoc());
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
                                                    llvm::IRBuilder<> &IRB, bool is_div) {
  if (!is_div) {
    return ConstantInt::getFalse(I.getContext());
  }

  Value *Divisor = I.getOperand(1);

  Value *Zero = ConstantFP::get(Divisor->getType(), 0.0);

  return IRB.CreateFCmpOEQ(Divisor, Zero);
}

Value *FPOverflowChecker::getSementicBoundCondition(llvm::BinaryOperator &I,
                                                    llvm::IRBuilder<> &IRB) {

  Value *Result = &I;
  return IRB.CreateFCmpOGT(
      Result, ConstantFP::get(Result->getType(), sementic_threshold));
}

Value *FPOverflowChecker::getSementicMinusBoundCondition(llvm::BinaryOperator &I,
                                                    llvm::IRBuilder<> &IRB) {

  Value *Result = &I;
  return IRB.CreateFCmpOLT(
      Result, ConstantFP::get(Result->getType(), -sementic_threshold));
}

Value *FPOverflowChecker::getSemnticMaxBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB) {
  Value *Result = &I;
  const fltSemantics &Sem = Result->getType()->getFltSemantics();
  APFloat MaxValue = APFloat::getLargest(Sem);

  bool LosesInfo;
  APFloat Tolerance(sementic_tolerance);  // double → IEEEdouble
  Tolerance.convert(Sem, APFloat::rmNearestTiesToEven, &LosesInfo);  // Sem으로 변환

  return IRB.CreateFCmpOGT(
      Result, ConstantFP::get(Result->getType(), MaxValue - Tolerance));
}

Value *FPOverflowChecker::getSementicMinBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB) {
  Value *Result = &I;
  const fltSemantics &Sem = Result->getType()->getFltSemantics();
  APFloat MinValue = APFloat::getSmallest(Sem);

  bool LosesInfo;
  APFloat Tolerance(sementic_tolerance);
  Tolerance.convert(Sem, APFloat::rmNearestTiesToEven, &LosesInfo);

  return IRB.CreateFCmpOLT(
      Result, ConstantFP::get(Result->getType(), MinValue + Tolerance));
}

Value *FPOverflowChecker::getSementicToleranceCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB) {
  Value *Result = &I;
  const fltSemantics &Sem = Result->getType()->getFltSemantics();

  bool LosesInfo;
  APFloat Tolerance(sementic_tolerance);
  Tolerance.convert(Sem, APFloat::rmNearestTiesToEven, &LosesInfo);

  APFloat NegTolerance = Tolerance;
  NegTolerance.changeSign();

  Value *ToleranceFP = ConstantFP::get(Result->getType(), Tolerance);
  Value *NegToleranceFP = ConstantFP::get(Result->getType(), NegTolerance);

  // -Tolerance < Result < Tolerance
  Value *LT = IRB.CreateFCmpOLT(Result, ToleranceFP);
  Value *GT = IRB.CreateFCmpOGT(Result, NegToleranceFP);
  return IRB.CreateAnd(LT, GT);
}

void FPOverflowChecker::setSementicThreshold(double sementic_threshold) {
  this->sementic_threshold = sementic_threshold;
}