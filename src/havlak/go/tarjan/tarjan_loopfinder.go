package tarjan

import (
	"loopfinder/cfg"
	"loopfinder/lsg"
)

type TarjanLoopFinder struct {
	CFG          *cfg.CFG
	LSG          *lsg.LSG
	index        int
	disc         map[*cfg.BasicBlock]int
	low          map[*cfg.BasicBlock]int
	onStack      map[*cfg.BasicBlock]bool
	stack        []*cfg.BasicBlock
	nodeLoopMap  map[*cfg.BasicBlock]*lsg.SimpleLoop
}

func NewTarjanLoopFinder(cfgraph *cfg.CFG, lsgraph *lsg.LSG) *TarjanLoopFinder {
	return &TarjanLoopFinder{
		CFG:         cfgraph,
		LSG:         lsgraph,
		index:       0,
		disc:        make(map[*cfg.BasicBlock]int),
		low:         make(map[*cfg.BasicBlock]int),
		onStack:     make(map[*cfg.BasicBlock]bool),
		nodeLoopMap: make(map[*cfg.BasicBlock]*lsg.SimpleLoop),
	}
}

func (t *TarjanLoopFinder) FindLoops() {
	if t.CFG.StartBasicBlock() == nil {
		return
	}

	// Initialize all nodes
	for _, bb := range t.CFG.BasicBlocks() {
		t.disc[bb] = -1
		t.low[bb] = -1
		t.onStack[bb] = false
	}

	// Start DFS from start node
	t.strongConnect(t.CFG.StartBasicBlock())

	// Calculate nesting levels
	t.LSG.CalculateNestingLevel()
}

func (t *TarjanLoopFinder) strongConnect(node *cfg.BasicBlock) {
	t.disc[node] = t.index
	t.low[node] = t.index
	t.index++
	t.stack = append(t.stack, node)
	t.onStack[node] = true

	// Visit all neighbors
	for _, w := range node.OutEdges() {
		if t.disc[w] == -1 {
			t.strongConnect(w)
			if t.low[node] > t.low[w] {
				t.low[node] = t.low[w]
			}
		} else if t.onStack[w] {
			if t.low[node] > t.disc[w] {
				t.low[node] = t.disc[w]
			}
		}
	}

	// If node is root of SCC
	if t.low[node] == t.disc[node] {
		var component []*cfg.BasicBlock
		var w *cfg.BasicBlock

		for {
			n := len(t.stack) - 1
			w = t.stack[n]
			t.stack = t.stack[:n]
			t.onStack[w] = false
			component = append(component, w)
			if w == node {
				break
			}
		}

		isLoop := len(component) > 1
		if !isLoop {
			for _, succ := range node.OutEdges() {
				if succ == node {
					isLoop = true
					break
				}
			}
		}

		if isLoop {
			header := t.findLoopHeader(component)
			_ = header // (currently unused like C++ version)

			loop := t.LSG.NewLoop()

			for _, n := range component {
				loop.AddNode(n)
				t.nodeLoopMap[n] = loop
			}

			t.LSG.AddLoop(loop)
		}
	}
}

func (t *TarjanLoopFinder) findLoopHeader(component []*cfg.BasicBlock) *cfg.BasicBlock {
	componentSet := make(map[*cfg.BasicBlock]bool)
	for _, node := range component {
		componentSet[node] = true
	}

	for _, node := range component {
		for _, pred := range node.InEdges() {
			if !componentSet[pred] {
				return node
			}
		}
	}

	// No external edges, return first
	return component[0]
}

func FindTarjanLoops(cfgraph *cfg.CFG, lsgraph *lsg.LSG) int {
	finder := NewTarjanLoopFinder(cfgraph, lsgraph)
	finder.FindLoops()
	return lsgraph.NumLoops()
}
