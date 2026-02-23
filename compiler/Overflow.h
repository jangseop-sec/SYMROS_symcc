#ifndef OVERFLOW_H
#define OVERFLOW_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/ValueMap.h>
#include <llvm/Support/raw_ostream.h>

#include "Runtime.h"

class OverflowChecker : public llvm::InstVisitor<OverflowChecker> {
public:
  explicit OverflowChecker(llvm::Module &M) : runtime(M) {}
  void visitBinaryOperator(llvm::BinaryOperator &I);
  void setSementicThreshold(int sementic_threshold);

  const Runtime runtime;

private:
  llvm::Value *getAddOverflowCondition(llvm::BinaryOperator &I,
                                       llvm::IRBuilder<> &IRB);
  llvm::Value *getSubOverflowCondition(llvm::BinaryOperator &I,
                                       llvm::IRBuilder<> &IRB);
  llvm::Value *getMulOverflowCondition(llvm::BinaryOperator &I,
                                       llvm::IRBuilder<> &IRB);
  llvm::Value *getDividedByZeroCondition(llvm::BinaryOperator &I,
                                         llvm::IRBuilder<> &IRB);
  llvm::Value *getSementicSignedBoundCondition(llvm::BinaryOperator &I,
                                               llvm::IRBuilder<> &IRB);
  llvm::Value *getSementicUnsignedBoundCondition(llvm::BinaryOperator &I,
                                                 llvm::IRBuilder<> &IRB);

  int sementic_threshold;
};

#endif