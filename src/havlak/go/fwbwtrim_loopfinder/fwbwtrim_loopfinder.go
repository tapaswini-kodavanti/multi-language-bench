
//======================================================
// Forward-Backward Trim Parallel Algorithm
//======================================================

//
// The parallel loop finding algorithm.
//
package fwbwtrim_loopfinder

import "container/list"

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
	pred := reachable(pivot, false)
	desc := reachable(pivot, true)

	scc := intersect(pred, desc)

	sccSet := make(map[int]bool)
	for _, bb := range scc {
		sccSet[bb.Name()] = true
	}

	predMinusSCC := filter(pred, scc)
	descMinusSCC := filter(desc, scc)
	rem := filter(filter(cfgraph.BasicBlocks(), pred), desc) // rem is CFG - pred - desc (pivot is included in pred and desc)


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
		for _, bb := range cfg.BasicBlocks() {
			if ((forward && bb.NumPred() == 0) || (!forward && bb.NumSucc() == 0)) {
				cfg.Remove(bb.Name())
			}
		}
		if !modified {
			return
		}
	}
}

func pickPivot(cfg *cfg.CFG) *cfg.BasicBlock { // use a different heuristic here later
	for _, bb := range cfg.BasicBlocks() {
		return bb;
	}
	return nil;
}

func reachable(start *cfg.BasicBlock, desc bool) map[int]*cfg.BasicBlock { // returns all nodes that are reachable from the start (pred or desc)
	visited := make(map[int]bool)
	var nodes []*cfg.BasicBlock
	var stack []*cfg.BasicBlock
	stack = append(stack, start) 

	// basic BFS to check for reachable nodes
	for len(stack) > 0 {
		index := len(stack) - 1
		node := stack[index]
		stack = stack[:index]

		if visited[node.Name()] {
			continue
		}

		// Process node
		visited[node.Name()] = true
		nodes = append(nodes, node)
		var neighbors *list.List
		if desc {
			neighbors = node.OutEdges()
		} else {
			neighbors = node.InEdges()
		}

		// manually add nodes from neighbords into stack
		listNode := neighbors.Front()
		for listNode != nil {
			stack = append(stack, listNode.Value.(*cfg.BasicBlock))
			listNode = listNode.Next()

		}
	}

	// Make mapping of node
	result := make(map[int]*cfg.BasicBlock)
	for _, bb := range nodes {
		result[bb.Name()] = bb
	}
	return result
}

func intersect(pred map[int]*cfg.BasicBlock, desc map[int]*cfg.BasicBlock) map[int]*cfg.BasicBlock {
	result := make(map[int]*cfg.BasicBlock)
	for _, bb := range pred {
		if _, exists := desc[bb.Name()]; exists {
			result[bb.Name()] = bb
		}
	}
	return result
}

func filter(initial map[int]*cfg.BasicBlock, remove map[int]*cfg.BasicBlock) map[int]*cfg.BasicBlock {
	result := make(map[int]*cfg.BasicBlock)
	for _, bb := range initial {
		if _, exists := remove[bb.Name()]; !exists {
			result[bb.Name()] = bb
		}
	}
	return result
}

func makeSubCFG(nodes map[int]*cfg.BasicBlock) *cfg.CFG{
	return cfg.NewCFGwithNodes(nodes)
}

func FindFWBWLoops(cfgraph *cfg.CFG, lsgraph *lsg.LSG) int { // Entry point
	FindLoops(cfgraph, lsgraph)
	return lsgraph.NumLoops()
}
