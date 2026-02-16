#include "Unroll.h"

using namespace llvm;

bool Unroll::unroll() {
  bool Changed = false;

  for (Loop *L : LI.getLoopsInPreorder()) {
    if (!L->getLoopPreheader())
      continue;
    if (L->getBlocks().size() > 20)
      continue;

    UnrollLoopOptions ULO;
    ULO.Count = 2;
    ULO.Force = true;
    ULO.Runtime = false;
    ULO.ForgetAllSCEV = false;

    Changed |= (UnrollLoop(L, ULO, &LI, &SE, &DT, &AC, &TTI, &ORE, false) !=
                LoopUnrollResult::Unmodified);
  }

  return Changed;
}