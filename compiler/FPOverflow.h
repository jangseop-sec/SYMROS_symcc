#ifndef FPOVERFLOW_H
#define FPOVERFLOW_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/ValueMap.h>
#include <llvm/Support/raw_ostream.h>

#include "Runtime.h"

class FPOverflowChecker : public llvm::InstVisitor<FPOverflowChecker> {
public:
  explicit FPOverflowChecker(llvm::Module &M) : runtime(M) {}
  void visitBinaryOperator(llvm::BinaryOperator &I);
  void setSementicThreshold(double sementic_threshold);
  void setSementicTolerance(double sementic_tolerance);

  const Runtime runtime;

private:
  llvm::Value *getOverflowCond(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB);
  llvm::Value *getDividedByZeroCondition(llvm::BinaryOperator &I,
                                         llvm::IRBuilder<> &IRB, bool is_div);
  llvm::Value *getSementicBoundCondition(llvm::BinaryOperator &I,
                                         llvm::IRBuilder<> &IRB);
  llvm::Value *getSementicMinusBoundCondition(llvm::BinaryOperator &I,
                                              llvm::IRBuilder<> &IRB);
  llvm::Value *getSemnticMaxBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB);
  llvm::Value *getSementicMinBoundCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB);
  llvm::Value *getSementicToleranceCondition(llvm::BinaryOperator &I, llvm::IRBuilder<> &IRB);

  double sementic_threshold;
  double sementic_tolerance;
};

#endif