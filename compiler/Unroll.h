#ifndef UNROLL_LOOP_H
#define UNROLL_LOOP_H

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/Transforms/Utils/UnrollLoop.h>

class Unroll {
public:
  explicit Unroll(llvm::LoopInfo &LI, llvm::ScalarEvolution &SE,
                  llvm::DominatorTree &DT, llvm::AssumptionCache &AC,
                  llvm::TargetTransformInfo &TTI,
                  llvm::OptimizationRemarkEmitter &ORE)
      : LI(LI), SE(SE), DT(DT), AC(AC), TTI(TTI), ORE(ORE) {}

  bool unroll();

private:
  llvm::LoopInfo &LI;
  llvm::ScalarEvolution &SE;
  llvm::DominatorTree &DT;
  llvm::AssumptionCache &AC;
  llvm::TargetTransformInfo &TTI;
  llvm::OptimizationRemarkEmitter &ORE;
};

#endif // UNROLL_LOOP_H