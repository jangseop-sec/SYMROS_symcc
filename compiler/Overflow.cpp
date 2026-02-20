#include "Overflow.h"

using namespace llvm;

void OverflowChecker::visitBinaryOperator(BinaryOperator &I) {

  if (!I.getOperand(0)->getType()->isIntegerTy(32) ||
      !I.getOperand(1)->getType()->isIntegerTy(32))
    return;

  if (I.hasNoSignedWrap() || I.hasNoUnsignedWrap())
    return;

  llvm::Value *overflowCond = nullptr;
  std::string tag = "int_overflow";

  if (I.getOpcode() == Instruction::Add) {
    overflowCond = getAddOverflowCondition(I);
  } else if (I.getOpcode() == Instruction::Sub) {
    overflowCond = getSubOverflowCondition(I);
  } else if (I.getOpcode() == Instruction::Mul) {
    overflowCond = getMulOverflowCondition(I);
  } else if (I.getOpcode() == Instruction::UDiv ||
             I.getOpcode() == Instruction::SDiv) {
    overflowCond = getDividedByZeroCondition(I);
    tag = "int_divided_by_zero";
  } else {
    return;
  }

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
  MDNode *Tag = MDNode::get(Ctx, MDString::get(Ctx, tag.c_str()));
  CreatedBr->setMetadata("symros.check", Tag);
}

Value *OverflowChecker::getAddOverflowCondition(BinaryOperator &I) {

  Value *a = I.getOperand(0);
  Value *b = I.getOperand(1);

  IRBuilder<> IRB(&I);
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

Value *OverflowChecker::getSubOverflowCondition(BinaryOperator &I) {
  Value *a = I.getOperand(0);
  Value *b = I.getOperand(1);

  LLVMContext &Ctx = I.getContext();

  IRBuilder<> IRB(&I); // ✅ split 전에 생성!

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

Value *OverflowChecker::getMulOverflowCondition(BinaryOperator &I) {
  Value *a = I.getOperand(0);
  Value *b = I.getOperand(1);

  LLVMContext &Ctx = I.getContext();

  IRBuilder<> IRB(&I); // ✅ 반드시 split 전에 생성

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

Value *OverflowChecker::getDividedByZeroCondition(llvm::BinaryOperator &I) {

  Value *Divisor = I.getOperand(1);

  IRBuilder<> IRB(&I);

  Value *Zero = ConstantInt::get(Divisor->getType(), 0);

  return IRB.CreateICmpEQ(Divisor, Zero);
}