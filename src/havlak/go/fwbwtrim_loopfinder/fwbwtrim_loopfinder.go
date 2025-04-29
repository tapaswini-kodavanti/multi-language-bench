//======================================================
// Forward-Backward Trim Parallel Algorithm
//======================================================

// The parallel loop finding algorithm.
package fwbwtrim_loopfinder

import (
	"loopfinder/cfg"
	"loopfinder/lsg"
	"sync"
)

// FindLoops
//
// Find loops and build loop forest using Forward-Backward Trim algorithm
//

// Each CFG is a wrapper around a mapping of node objects (BasicBlocks)
// In each recursive iteration, create new CFGs that use the same building blocks as originally created
// To prune, remove the BasicBlock from the list, don't delete the object

func FindLoops(cfgraph *cfg.CFG, lsgraph *lsg.LSG, loopMap map[*cfg.BasicBlock]*lsg.SimpleLoop, mu *sync.Mutex) { 
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
	if cfgraph.NumNodes() == 0 { // all nodes deleted in time
		return
	}

	pivot := pickPivot(cfgraph)
	pred := reachable(pivot, false)
	desc := reachable(pivot, true)

	scc := intersect(pred, desc)

	predMinusSCC := filter(pred, scc)
	descMinusSCC := filter(desc, scc)
	rem := filter(filter(cfgraph.BasicBlocks(), pred), desc) // rem is CFG - pred - desc (pivot is included in pred and desc)

	FindLoops(makeSubCFG(predMinusSCC), lsgraph, loopMap, mu)
	FindLoops(makeSubCFG(descMinusSCC), lsgraph, loopMap, mu)
	FindLoops(makeSubCFG(rem), lsgraph, loopMap, mu)

	var wg sync.WaitGroup
	wg.Add(3)

	go func() {
		defer wg.Done()
		FindLoops(makeSubCFG(predMinusSCC), lsgraph, loopMap, mu)
	}()

	go func() {
		defer wg.Done()
		FindLoops(makeSubCFG(descMinusSCC), lsgraph, loopMap, mu)
	}()

	go func() {
		defer wg.Done()
		FindLoops(makeSubCFG(rem), lsgraph, loopMap, mu)
	}()

	wg.Wait()


	// loop through all identified SCCs
	// determine if reducible
	// do same thing as havlak paper to add to final lsgraph

	// Create loop descriptor for the SCC if it's meaningful
	if len(scc) > 0 {
		loop := lsgraph.NewLoop()
		loop.SetHeader(pivot)

		isReducible := isLoopReducible(scc, pivot)
		loop.SetIsReducible(isReducible)

		// ensure nesting of parent loops
		mu.Lock()
		for _, bb := range scc {
			if _, inLoop := loopMap[bb]; inLoop && loopMap[bb] != loop {
				if loopMap[bb] != loop {
					loop.AddChildLoop(loopMap[bb])
				}
			} else {
				loop.AddNode(bb)
				loopMap[bb] = loop
			}
		}
		mu.Unlock()
		lsgraph.AddLoop(loop)
	}

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
				modified = true

				if forward {
					// If forward, remove the node from the outEdges of its predecessors
					for _, pred := range bb.InEdges() {
						pred.RemoveEdge(bb.Name()) // Remove bb from pred's outEdges
					}
				} else {
					// If backward, remove the node from the inEdges of its successors
					for _, succ := range bb.OutEdges() {
						succ.RemoveEdge(bb.Name()) // Remove bb from succ's inEdges
					}
				}
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
		var neighbors map[int]*cfg.BasicBlock
		if desc {
			neighbors = node.OutEdges()
		} else {
			neighbors = node.InEdges()
		}

		// manually add nodes from neighbords into stack
		for _, listNode := range neighbors {
			stack = append(stack, listNode)
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

func isLoopReducible(scc map[int]*cfg.BasicBlock, pivot *cfg.BasicBlock) bool {
	for _, bb := range scc {
		// for each node's predecesor, check if it is within the SCC
		// if it is not and the node in question is not the pivot, then it implies that there are
		// multiple entry points, thus making the graph irreducible
		for _, predBB := range bb.InEdges() {
			if _, inSCC := scc[predBB.Name()]; !inSCC {
				if bb.Name() != pivot.Name() {
					return false
				}
			}
		}
	}
	return true
} 

func FindFWBWLoops(cfgraph *cfg.CFG, lsgraph *lsg.LSG) int { // Entry point
	var mu sync.Mutex
	loopMap := make(map[*cfg.BasicBlock]*lsg.SimpleLoop)
	FindLoops(cfgraph, lsgraph, loopMap, &mu)
	return lsgraph.NumLoops()
}
