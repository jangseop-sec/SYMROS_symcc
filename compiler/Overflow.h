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

  const Runtime runtime;

private:
  llvm::Value *getAddOverflowCondition(llvm::BinaryOperator &I);
  llvm::Value *getSubOverflowCondition(llvm::BinaryOperator &I);
  llvm::Value *getMulOverflowCondition(llvm::BinaryOperator &I);
  llvm::Value *getDividedByZeroCondition(llvm::BinaryOperator &I);
};

#endif