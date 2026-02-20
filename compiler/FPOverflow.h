#ifndef FPOVERFLOW_H
#define FPOVERFLOW_H

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/ValueMap.h>
#include <llvm/Support/raw_ostream.h>

#include "Runtime.h"

class FPOverflowChecker : public llvm::InstVisitor<FPOverflowChecker> {
public:
  explicit FPOverflowChecker(llvm::Module &M) : runtime(M) {}
  void visitBinaryOperator(llvm::BinaryOperator &I);

  const Runtime runtime;

private:
  llvm::Value *getOverflowCond(llvm::BinaryOperator &I);
  llvm::Value *getDevidedByZeroCondition(llvm::BinaryOperator &I);
};

#endif