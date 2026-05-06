#include "Overflow.h"

using namespace llvm;

void OverflowChecker::visitBinaryOperator(BinaryOperator &I) {

  if (!I.getOperand(0)->getType()->isIntegerTy(32) ||
      !I.getOperand(1)->getType()->isIntegerTy(32))
    return;

  // if (I.hasNoSignedWrap() || I.hasNoUnsignedWrap())
  //   return;

  if (I.getOpcode() != Instruction::Add && I.getOpcode() != Instruction::Sub &&
      I.getOpcode() != Instruction::Mul && I.getOpcode() != Instruction::UDiv &&
      I.getOpcode() != Instruction::SDiv) {
    return;
  }

  // TODO implement!!!
  LLVMContext &Ctx = I.getContext();
  Function *F = I.getFunction();
  BasicBlock *OriginBB = I.getParent();

  Instruction *NextInst = I.getNextNode();
  if (!NextInst)
    return;

  BasicBlock *ContBB = OriginBB->splitBasicBlock(NextInst, "orgin_cont");

  OriginBB->getTerminator()->eraseFromParent();

  // basic block construction (overflow / signed bound / unsigned bound)
  BasicBlock *OverflowCheckBB = BasicBlock::Create(Ctx, "overflow_check", F);
  BasicBlock *SignedBoundCheckBB =
      BasicBlock::Create(Ctx, "signed_bound_check", F);
  BasicBlock *UnsignedBoundCheckBB =
      BasicBlock::Create(Ctx, "unsigned_bound_check", F);
  BasicBlock *SignedMinusBoundCheckBB =
      BasicBlock::Create(Ctx, "signed_minus_bound_check", F);
  BasicBlock *MaxBoundCheckBB =
      BasicBlock::Create(Ctx, "max_bound_check", F);
  BasicBlock *MinBoundCheckBB =
      BasicBlock::Create(Ctx, "min_bound_check", F);
  BasicBlock *SignedMaxBoundCheckBB =
      BasicBlock::Create(Ctx, "signed_max_bound_check", F);
  BasicBlock *SignedMinBoundCheckBB =
      BasicBlock::Create(Ctx, "signed_min_bound_check", F);

  IRBuilder<> IRB(OriginBB);
  IRB.CreateBr(OverflowCheckBB);

  IRBuilder<> IR1(OverflowCheckBB);

  // overflow check branch
  Value *OverflowCond = nullptr;
  std::string overflow_tag = "int_overflow";

  switch (I.getOpcode()) {
  case Instruction::Add:
    OverflowCond = getAddOverflowCondition(I, IR1);
    break;
  case Instruction::Sub:
    OverflowCond = getSubOverflowCondition(I, IR1);
    break;
  case Instruction::Mul:
    OverflowCond = getMulOverflowCondition(I, IR1);
    break;
  case Instruction::SDiv:
  case Instruction::UDiv:
    overflow_tag = "int_divided_by_zero";
    OverflowCond = getDividedByZeroCondition(I, IR1);
    break;
  default:
    break;
  }

  if (!OverflowCond)
    return;

  BranchInst *OverflowCheckBranch =
      IR1.CreateCondBr(OverflowCond, SignedBoundCheckBB, SignedBoundCheckBB);

  // set metadata
  OverflowCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, overflow_tag.c_str())));
  OverflowCheckBranch->setDebugLoc(I.getDebugLoc());

  // signed bound check branch
  IRBuilder<> IR2(SignedBoundCheckBB);

  Value *SignedBoundCond = getSementicSignedBoundCondition(I, IR2);

  BranchInst *SignedBoundCheckBranch = IR2.CreateCondBr(
      SignedBoundCond, UnsignedBoundCheckBB, UnsignedBoundCheckBB);

  // set metadata
  SignedBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "int_exceptional_signed_value")));
  SignedBoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // unsigned bound check branch
  IRBuilder<> IR3(UnsignedBoundCheckBB);

  Value *UnsignedBoundCond = getSementicUnsignedBoundCondition(I, IR3);

  BranchInst *UnsignedBoundCheckBranch =
      IR3.CreateCondBr(UnsignedBoundCond, SignedMinusBoundCheckBB, SignedMinusBoundCheckBB);

  // set metadata
  UnsignedBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "int_exceptional_unsigned_value")));
  UnsignedBoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // signed minus bound check branch
  IRBuilder<> IR4(SignedMinusBoundCheckBB);
  Value *SignedMinusBoundCond = getSementicSignedMinusBoundCondition(I, IR4);
  BranchInst *SignedMinusBoundCheckBranch =
      IR4.CreateCondBr(SignedMinusBoundCond, MaxBoundCheckBB, MaxBoundCheckBB);
  
  // set metadata
  SignedMinusBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "int_exceptional_signed_minus_value")));
  SignedMinusBoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // max bound check branch
  IRBuilder<> IR5(MaxBoundCheckBB);
  Value *MaxBoundCond = getSementicMaxBoundCondition(I, IR5);
  BranchInst *MaxBoundCheckBranch =
      IR5.CreateCondBr(MaxBoundCond, SignedMaxBoundCheckBB, SignedMaxBoundCheckBB);

  // set metadata
  MaxBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "int_exceptional_max_value")));
  MaxBoundCheckBranch->setDebugLoc(I.getDebugLoc());  

  // min bound check branch
  IRBuilder<> IR6(MinBoundCheckBB);
  Value *MinBoundCond = getSementicMinBoundCondition(I, IR6);
  BranchInst *MinBoundCheckBranch =
      IR6.CreateCondBr(MinBoundCond, SignedMaxBoundCheckBB, SignedMaxBoundCheckBB); 
  
  // set metadata
  MinBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "int_exceptional_min_value")));
  MinBoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // signed max bound check branch
  IRBuilder<> IR7(SignedMaxBoundCheckBB);
  Value *SignedMaxBoundCond = getSementicSignedMaxBoundCondition(I, IR7);
  BranchInst *SignedMaxBoundCheckBranch =
      IR7.CreateCondBr(SignedMaxBoundCond, SignedMinBoundCheckBB, SignedMinBoundCheckBB);
  
  // set metadata
  SignedMaxBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "int_exceptional_signed_max_value")));
  SignedMaxBoundCheckBranch->setDebugLoc(I.getDebugLoc());

  // signed min bound check branch
  IRBuilder<> IR8(SignedMinBoundCheckBB);
  Value *SignedMinBoundCond = getSementicSignedMinBoundCondition(I, IR8);
  BranchInst *SignedMinBoundCheckBranch =
      IR8.CreateCondBr(SignedMinBoundCond, ContBB, ContBB);
  
  // set metadata
  SignedMinBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "int_exceptional_signed_min_value")));
  SignedMinBoundCheckBranch->setDebugLoc(I.getDebugLoc());
}

Value *OverflowChecker::getAddOverflowCondition(BinaryOperator &I,
                                                IRBuilder<> &IRB) {

  Value *a = I.getOperand(0);
  Value *b = I.getOperand(1);

  LLVMContext &Ctx = I.getContext();
  Type *Ty = a->getType();

  assert(Ty->isIntegerTy() && "only integer add supported");

  /*
   * signed overflow
   */
  unsigned BitWidth = Ty->getIntegerBitWidth();

  APInt IntMax = APInt::getSignedMaxValue(BitWidth); // INT_MAX
  APInt IntMin = APInt::getSignedMinValue(BitWidth); // INT_MIN

  Value *INT_MAX_V = ConstantInt::get(Ctx, IntMax);
  Value *INT_MIN_V = ConstantInt::get(Ctx, IntMin);
  Value *ZERO = ConstantInt::get(Ty, 0);

  // cond: (b > 0 && a > INT_MAX - b)
  Value *b_gt_0 = IRB.CreateICmpSGT(b, ZERO);
  Value *intmax_minus_b = IRB.CreateSub(INT_MAX_V, b);
  Value *a_gt_intmax_minus_b = IRB.CreateICmpSGT(a, intmax_minus_b);
  Value *pos_overflow = IRB.CreateAnd(b_gt_0, a_gt_intmax_minus_b);

  // cond: (b < 0 && a < INT_MIN - b)
  Value *b_lt_0 = IRB.CreateICmpSLT(b, ZERO);
  Value *intmin_minus_b = IRB.CreateSub(INT_MIN_V, b);
  Value *a_lt_intmin_minus_b = IRB.CreateICmpSLT(a, intmin_minus_b);
  Value *neg_overflow = IRB.CreateAnd(b_lt_0, a_lt_intmin_minus_b);

  // cond: (pos || neg)
  Value *signed_overflow = IRB.CreateOr(pos_overflow, neg_overflow);

  /*
   * unsigned overflow
   */
  Value *unsigned_sum = IRB.CreateAdd(a, b);
  // cond: (a + b) < a
  Value *unsigned_overflow = IRB.CreateICmpULT(unsigned_sum, a);

  /*
   * final overflow condition
   */
  return IRB.CreateOr(signed_overflow, unsigned_overflow);
}

Value *OverflowChecker::getSubOverflowCondition(BinaryOperator &I,
                                                IRBuilder<> &IRB) {
  Value *a = I.getOperand(0);
  Value *b = I.getOperand(1);

  LLVMContext &Ctx = I.getContext();

  Type *Ty = a->getType();
  assert(Ty->isIntegerTy() && "only integer sub supported");
  unsigned BitWidth = Ty->getIntegerBitWidth();

  // ============================
  // signed overflow condition
  // ============================

  APInt IntMax = APInt::getSignedMaxValue(BitWidth);
  APInt IntMin = APInt::getSignedMinValue(BitWidth);

  Value *INT_MAX_V = ConstantInt::get(Ctx, IntMax);
  Value *INT_MIN_V = ConstantInt::get(Ctx, IntMin);
  Value *ZERO = ConstantInt::get(Ty, 0);

  // (b < 0)
  Value *b_lt_0 = IRB.CreateICmpSLT(b, ZERO);

  // (a > INT_MAX + b)
  Value *intmax_plus_b = IRB.CreateAdd(INT_MAX_V, b);
  Value *a_gt_intmax_plus_b = IRB.CreateICmpSGT(a, intmax_plus_b);
  Value *pos_overflow = IRB.CreateAnd(b_lt_0, a_gt_intmax_plus_b);

  // (b > 0)
  Value *b_gt_0 = IRB.CreateICmpSGT(b, ZERO);

  // (a < INT_MIN + b)
  Value *intmin_plus_b = IRB.CreateAdd(INT_MIN_V, b);
  Value *a_lt_intmin_plus_b = IRB.CreateICmpSLT(a, intmin_plus_b);
  Value *neg_overflow = IRB.CreateAnd(b_gt_0, a_lt_intmin_plus_b);

  Value *signed_overflow = IRB.CreateOr(pos_overflow, neg_overflow);

  // ============================
  // unsigned overflow condition
  // ============================

  // a < b  → unsigned underflow
  Value *unsigned_overflow = IRB.CreateICmpULT(a, b);

  // ============================
  // final condition
  // ============================
  return IRB.CreateOr(signed_overflow, unsigned_overflow);
}

Value *OverflowChecker::getMulOverflowCondition(BinaryOperator &I,
                                                IRBuilder<> &IRB) {
  Value *a = I.getOperand(0);
  Value *b = I.getOperand(1);

  LLVMContext &Ctx = I.getContext();

  Type *Ty = a->getType();
  assert(Ty->isIntegerTy() && "only integer mul supported");
  unsigned BitWidth = Ty->getIntegerBitWidth();

  // ============================
  // signed overflow
  // ============================

  Value *ZERO = ConstantInt::get(Ty, 0);

  // for safe check
  Value *a_ne_0 = IRB.CreateICmpNE(a, ZERO);

  Value *prod = IRB.CreateMul(a, b);

  // a == 0이면 divisor를 1로 대체 → SIGFPE 방지
  Value *safe_a = IRB.CreateSelect(a_ne_0, a, ConstantInt::get(Ty, 1));
  Value *div = IRB.CreateSDiv(prod, safe_a);  // a==0일 땐 1로 나눔
  Value *div_ne_b = IRB.CreateICmpNE(div, b);
  Value *signed_overflow = IRB.CreateAnd(a_ne_0, div_ne_b);


  // ============================
  // Unsigned overflow
  // ============================

  APInt UIntMax = APInt::getMaxValue(BitWidth);
  Value *UINT_MAX_V = ConstantInt::get(Ctx, UIntMax);

  Value *b_ne_0 = IRB.CreateICmpNE(b, ZERO);
  Value *safe_b = IRB.CreateSelect(b_ne_0, b, ConstantInt::get(Ty, 1));
  Value *umax_div_b = IRB.CreateUDiv(UINT_MAX_V, safe_b);  // b==0일 땐 1로 나눔
  Value *unsigned_overflow = IRB.CreateAnd(b_ne_0, IRB.CreateICmpUGT(a, umax_div_b));


  // ============================
  // final condtion
  // ============================

  return IRB.CreateOr(signed_overflow, unsigned_overflow);
}

Value *OverflowChecker::getDividedByZeroCondition(llvm::BinaryOperator &I,
                                                  IRBuilder<> &IRB) {

  Value *Divisor = I.getOperand(1);

  Value *Zero = ConstantInt::get(Divisor->getType(), 0);

  return IRB.CreateICmpEQ(Divisor, Zero);
}

Value *OverflowChecker::getSementicSignedBoundCondition(llvm::BinaryOperator &I,
                                                        IRBuilder<> &IRB) {
  Value *Result = &I;
  return IRB.CreateICmpSGT(
      Result, ConstantInt::get(Result->getType(), sementic_threshold));
}

Value *
OverflowChecker::getSementicUnsignedBoundCondition(llvm::BinaryOperator &I,
                                                   IRBuilder<> &IRB) {

  Value *Result = &I;

  return IRB.CreateICmpUGT(
      Result, ConstantInt::get(Result->getType(), sementic_threshold));
}

Value*
OverflowChecker::getSementicSignedMinusBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB) {
  Value *Result = &I;

  return IRB.CreateICmpSLT(
      Result, ConstantInt::get(Result->getType(), -sementic_threshold));

}

Value *
OverflowChecker::getSementicMaxBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB) {
  Value *Result = &I;

  unsigned int max_value = 4294967294;
  return IRB.CreateICmpUGT(
      Result, ConstantInt::get(Result->getType(), max_value - sementic_tolerance));
}

Value *OverflowChecker::getSementicMinBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB) {
  Value *Result = &I;
  // APInt MinValue = APInt::getMinValue(Result->getType()->getIntegerBitWidth());
  return IRB.CreateICmpULT(
      Result, ConstantInt::get(Result->getType(), sementic_tolerance));
}

Value *OverflowChecker::getSementicSignedMaxBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB) {
  Value *Result = &I;

  APInt SignedMaxValue = APInt::getSignedMaxValue(Result->getType()->getIntegerBitWidth());
  return IRB.CreateICmpSGT(
      Result, ConstantInt::get(Result->getType(), 2147483646 - sementic_tolerance));
}

Value *OverflowChecker::getSementicSignedMinBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB) {
  Value *Result = &I;
  APInt SignedMinValue = APInt::getSignedMinValue(Result->getType()->getIntegerBitWidth());
  return IRB.CreateICmpSLT(
      Result, ConstantInt::get(Result->getType(), -2147483647 + sementic_tolerance));
}


void OverflowChecker::setSementicTolerance(int sementic_tolerance) {
  this->sementic_threshold = sementic_tolerance;
}

void OverflowChecker::setSementicThreshold(int sementic_threshold) {
  this->sementic_threshold = sementic_threshold;
}