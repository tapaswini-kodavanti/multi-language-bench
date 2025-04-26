#ifndef FWBW_LOOPS_H_
#define FWBW_LOOPS_H_

#include "mao-loops.h"

// forward declaration of the FWBWLoopFinder class
class FWBWLoopFinder;

// entry point for FWBW Trim algorithm
int FindFWBWLoops(MaoCFG *CFG, LoopStructureGraph *LSG);

#endif // FWBW_LOOPS_H_
