#include "Overflow.h"

using namespace llvm;

void OverflowChecker::visitBinaryOperator(BinaryOperator &I) {

  if (!I.getOperand(0)->getType()->isIntegerTy(32) ||
      !I.getOperand(1)->getType()->isIntegerTy(32))
    return;

  if (I.hasNoSignedWrap() || I.hasNoUnsignedWrap())
    return;

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
      IR3.CreateCondBr(UnsignedBoundCond, ContBB, ContBB);

  // set metadata
  UnsignedBoundCheckBranch->setMetadata(
      "symros.check",
      MDNode::get(Ctx, MDString::get(Ctx, "int_exceptional_unsigned_value")));
  UnsignedBoundCheckBranch->setDebugLoc(I.getDebugLoc());
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

  // a != 0
  Value *a_ne_0 = IRB.CreateICmpNE(a, ZERO);

  // prod = a * b
  Value *prod = IRB.CreateMul(a, b);

  // (prod / a)
  Value *div = IRB.CreateSDiv(prod, a);

  // (prod / a != b)
  Value *signed_overflow = IRB.CreateAnd(a_ne_0, IRB.CreateICmpNE(div, b));

  // ============================
  // Unsigned overflow
  // ============================

  APInt UIntMax = APInt::getMaxValue(BitWidth);
  Value *UINT_MAX_V = ConstantInt::get(Ctx, UIntMax);

  // b != 0
  Value *b_ne_0 = IRB.CreateICmpNE(b, ZERO);

  // UINT_MAX / b
  Value *umax_div_b = IRB.CreateUDiv(UINT_MAX_V, b);

  // a > (UINT_MAX / b)
  Value *unsigned_overflow =
      IRB.CreateAnd(b_ne_0, IRB.CreateICmpUGT(a, umax_div_b));

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

  Value *LowerBound = IRB.CreateICmpSLT(
      Result, ConstantInt::get(Result->getType(), -sementic_threshold));
  Value *UpperBound = IRB.CreateICmpSGT(
      Result, ConstantInt::get(Result->getType(), sementic_threshold));

  return IRB.CreateOr(LowerBound, UpperBound);
}

Value *
OverflowChecker::getSementicUnsignedBoundCondition(llvm::BinaryOperator &I,
                                                   IRBuilder<> &IRB) {

  Value *Result = &I;

  return IRB.CreateICmpUGT(
      Result, ConstantInt::get(Result->getType(), sementic_threshold));
}

void OverflowChecker::setSementicThreshold(int sementic_threshold) {
  this->sementic_threshold = sementic_threshold;
}