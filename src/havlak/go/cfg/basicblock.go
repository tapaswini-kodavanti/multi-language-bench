// Copyright 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//======================================================
// Scaffold Code
//======================================================

// A simple class simulating the concept of Basic Blocks
//
package cfg

import "fmt"

// type BasicBlock struct {
// 	name     int
// 	inEdges  list.List
// 	outEdges list.List
// }

// func NewBasicBlock(name int) *BasicBlock {
// 	return &BasicBlock{name: name}
// }

// func (bb *BasicBlock) Dump() {
// 	fmt.Printf("BB#%03d: ", bb.Name())
// 	if bb.NumPred() > 0 {
// 		fmt.Printf("in : ")
// 		for iter := bb.InEdges().Front(); iter != nil; iter = iter.Next() {
// 			fmt.Printf("BB#%03d ", iter.Value.(*BasicBlock).Name())
// 		}
// 	}
// 	if bb.NumSucc() > 0 {
// 		fmt.Print("out: ")
// 		for iter := bb.OutEdges().Front(); iter != nil; iter = iter.Next() {
// 			fmt.Printf("BB#%03d ", iter.Value.(*BasicBlock).Name())
// 		}
// 	}
// 	fmt.Printf("\n")
// }

// func (bb *BasicBlock) Name() int {
// 	return bb.name
// }

// func (bb *BasicBlock) InEdges() *list.List {
// 	return &bb.inEdges
// }

// func (bb *BasicBlock) OutEdges() *list.List {
// 	return &bb.outEdges
// }

// func (bb *BasicBlock) NumPred() int {
// 	return bb.inEdges.Len()
// }

// func (bb *BasicBlock) NumSucc() int {
// 	return bb.outEdges.Len()
// }

// func (bb *BasicBlock) AddInEdge(from *BasicBlock) {
// 	bb.inEdges.PushBack(from)
// }

// func (bb *BasicBlock) AddOutEdge(to *BasicBlock) {
// 	bb.outEdges.PushBack(to)
// }


type BasicBlock struct {
	name     int
	inEdges  map[int]*BasicBlock // Mapping of block ID to BasicBlock
	outEdges map[int]*BasicBlock // Mapping of block ID to BasicBlock
}

func NewBasicBlock(name int) *BasicBlock {
	return &BasicBlock{
		name:     name,
		inEdges:  make(map[int]*BasicBlock),
		outEdges: make(map[int]*BasicBlock),
	}
}

func (bb *BasicBlock) Dump() {
	fmt.Printf("BB#%03d: ", bb.Name())
	if bb.NumPred() > 0 {
		fmt.Printf("in : ")
		for _, pred := range bb.InEdges() {
			fmt.Printf("BB#%03d ", pred.Name())
		}
	}
	if bb.NumSucc() > 0 {
		fmt.Print("out: ")
		for _, succ := range bb.OutEdges() {
			fmt.Printf("BB#%03d ", succ.Name())
		}
	}
	fmt.Printf("\n")
}

func (bb *BasicBlock) Name() int {
	return bb.name
}

func (bb *BasicBlock) InEdges() map[int]*BasicBlock {
	return bb.inEdges
}

func (bb *BasicBlock) OutEdges() map[int]*BasicBlock {
	return bb.outEdges
}

func (bb *BasicBlock) NumPred() int {
	return len(bb.inEdges)
}

func (bb *BasicBlock) NumSucc() int {
	return len(bb.outEdges)
}

func (bb *BasicBlock) AddInEdge(from *BasicBlock) {
	bb.inEdges[from.Name()] = from
}

func (bb *BasicBlock) AddOutEdge(to *BasicBlock) {
	bb.outEdges[to.Name()] = to
}

// RemoveEdge removes the edge from the given id in either the inEdges or outEdges map
func (bb *BasicBlock) RemoveEdge(id int) {
	delete(bb.inEdges, id)
	delete(bb.outEdges, id)
}

//-----------------------------------------------------------

type CFG struct {
	bb        map[int]*BasicBlock
	startNode *BasicBlock
}

func NewCFG() *CFG {
	return &CFG{bb: make(map[int]*BasicBlock)}
}

// func NewCFGwithNodes(nodes map[int]*BasicBlock) *CFG {
// 	return &CFG{bb: nodes}
// }

// NewCFGwithNodes duplicates the nodes and edges, returning a new graph
func NewCFGwithNodes(nodes map[int]*BasicBlock) *CFG {
	// Create a new map for the new graph's nodes
	newNodes := make(map[int]*BasicBlock)

	// Create new BasicBlock instances for each original node
	for _, block := range nodes {
		// Create a new BasicBlock
		newBlock := NewBasicBlock(block.Name())
		newNodes[newBlock.Name()] = newBlock
	}

	// Now duplicate the edges between the nodes
	for id, block := range nodes {
		// Get the new block for this node
		newBlock := newNodes[id]

		// Copy in-edges
		for _, pred := range block.inEdges {
			// Get the new predecessor block
			newPred := newNodes[pred.Name()] // only add nodes that are in the subgraph
			if newPred != nil {
				newBlock.AddInEdge(newPred)
			}
		}

		// Copy out-edges
		for _, succ := range block.outEdges {
			// Get the new successor block
			newSucc := newNodes[succ.Name()]
			if newSucc != nil {
				newBlock.AddOutEdge(newSucc)
			}
		}
	}

	// Return a new CFG with the duplicated nodes
	return &CFG{bb: newNodes}
}

func (cfg *CFG) BasicBlocks() map[int]*BasicBlock {
	return cfg.bb
}

func (cfg *CFG) NumNodes() int {
	return len(cfg.bb)
}

func (cfg *CFG) CreateNode(node int) *BasicBlock {
	if bblock := cfg.bb[node]; bblock != nil {
		return bblock
	}
	bblock := NewBasicBlock(node)
	cfg.bb[node] = bblock

	if cfg.NumNodes() == 1 {
		cfg.startNode = bblock
	}

	return bblock
}

func (cfg *CFG) Dump() {
	for _, n := range cfg.bb {
		n.Dump()
	}
}

func (cfg *CFG) StartBasicBlock() *BasicBlock {
	return cfg.startNode
}

func (cfg *CFG) Dst(edge *BasicBlockEdge) *BasicBlock {
	return edge.Dst()
}

func (cfg *CFG) Src(edge *BasicBlockEdge) *BasicBlock {
	return edge.Src()
}

func (cfg *CFG) Remove(id int) {
	delete(cfg.bb, id)
}

//-----------------------------------------------------------

type BasicBlockEdge struct {
	to   *BasicBlock
	from *BasicBlock
}

func (edge *BasicBlockEdge) Dst() *BasicBlock {
	return edge.to
}

func (edge *BasicBlockEdge) Src() *BasicBlock {
	return edge.from
}

func NewBasicBlockEdge(cfg *CFG, from int, to int) *BasicBlockEdge {
	self := new(BasicBlockEdge)
	self.to = cfg.CreateNode(to)
	self.from = cfg.CreateNode(from)

	self.from.AddOutEdge(self.to)
	self.to.AddInEdge(self.from)

	return self
}

//-----------------------------------------------------------
