
//======================================================
// Forward-Backward Trim Parallel Algorithm
//======================================================

//
// The parallel loop finding algorithm.
//
package fwbwtrim_loopfinder

// import "container/list"

import (
    "loopfinder/cfg"
    "loopfinder/lsg"
)

// FindLoops
//
// Find loops and build loop forest using Forward-Backward Trim algorithm
//

// Each CFG is a wrapper around a mapping of node objects (BasicBlocks)
// In each recursive iteration, create new CFGs that use the same building blocks as originally created
// To prune, remove the BasicBlock from the list, don't delete the object

func FindLoops(cfgraph *cfg.CFG, lsgraph *lsg.LSG) { 
	// if cfgraph has 1 vertex or less, return (no more loops)

	// apply forward trim to considering nodes to further prune out unnecessary ones (need a union find...)
	// apply backward trim
	// take intersection of these sets; this is the SCC which you track
	// if there are still nodes, pick a partition node
	// do topo sort to find predecesors and descendants
	// call this function 

	if cfgraph.NumNodes() <= 1 { // no more loops to detect in here
		return
	}

	trimForward(cfgraph)
	if cfgraph.NumNodes() == 0 { // all nodes deleted in time
		return
	}
	trimBackward(cfgraph)

	pivot := pickPivot(cfgraph)
	pred := reachable(cfgraph, pivot, false)
	desc := reachable(cfgraph, pivot, true)

	scc := intersect(pred, desc)

	sccSet := make(map[int]bool)
	for _, b := range scc {
		sccSet[b.ID] = true
		// delete(cfg.BB, b.ID)   ;    why do we need to do this?
	}

	predMinusSCC := filter(pred, sccSet)
	descMinusSCC := filter(desc, sccSet)
	rem := filter(filter(filter(cfgraph.BasicBlocks(), pred), desc), []*cfg.BasicBlock{pivot}) // rem is CFG - pred - desc - pivot


	FindLoops(makeSubCFG(predMinusSCC), lsgraph)
	FindLoops(makeSubCFG(descMinusSCC), lsgraph)
	FindLoops(makeSubCFG(rem), lsgraph)


	// loop through all identified SCCs
	// determine if reducible
	// do same thing as havlak paper to add to final lsgraph
}

func trimForward(cfg *cfg.CFG) {
	trim(cfg, true)
}

func trimBackward(cfg *cfg.CFG) {
	trim(cfg, false)
}

func trim(cfg *cfg.CFG, forward bool) {
	var modified bool
	for { // loop until there are no more predecessors to remove
		modified = false
		for id, bb := range cfg.BasicBlocks() {
			if ((forward && bb.NumPred() == 0) || (!forward && bb.NumSucc() == 0)) {
				cfg.Remove(id)
			}
		}
		if !modified {
			return
		}
	}
}

func FindFWBWLoops(cfgraph *cfg.CFG, lsgraph *lsg.LSG) int { // Entry point
	FindLoops(cfgraph, lsgraph)
	return lsgraph.NumLoops()
}
