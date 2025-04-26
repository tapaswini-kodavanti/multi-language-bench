#ifndef TARJAN_LOOPS_H_
#define TARJAN_LOOPS_H_

#include "mao-loops.h"

// forward declaration of the TarjanLoopFinder class
class TarjanLoopFinder;

// entry point for Tarjan's algorithm
int FindTarjanLoops(MaoCFG *CFG, LoopStructureGraph *LSG);

#endif // TARJAN_LOOPS_H_
