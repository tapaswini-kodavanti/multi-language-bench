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

// Test Program for the Havlak loop finder.
//
// This program constructs a fairly large control flow
// graph and performs loop recognition. This is the Go
// version.
package main

import (
	"fmt"
	"loopfinder/cfg"
	"loopfinder/fwbwtrim_loopfinder"
	// "loopfinder/havlakloopfinder"
	"loopfinder/lsg"
	"loopfinder/tarjan"
	"time"
)

// import "./basicblock"

//======================================================
// Testing Code
//======================================================

func buildDiamond(cfgraph *cfg.CFG, start int) int {
	bb0 := start
	cfg.NewBasicBlockEdge(cfgraph, bb0, bb0+1)
	cfg.NewBasicBlockEdge(cfgraph, bb0, bb0+2)
	cfg.NewBasicBlockEdge(cfgraph, bb0+1, bb0+3)
	cfg.NewBasicBlockEdge(cfgraph, bb0+2, bb0+3)

	return bb0 + 3
}

func buildConnect(cfgraph *cfg.CFG, start int, end int) {
	cfg.NewBasicBlockEdge(cfgraph, start, end)
}

func buildStraight(cfgraph *cfg.CFG, start int, n int) int {
	for i := 0; i < n; i++ {
		buildConnect(cfgraph, start+i, start+i+1)
	}
	return start + n
}

func buildBaseLoop(cfgraph *cfg.CFG, from int) int {
	header := buildStraight(cfgraph, from, 1)
	diamond1 := buildDiamond(cfgraph, header)
	d11 := buildStraight(cfgraph, diamond1, 1)
	diamond2 := buildDiamond(cfgraph, d11)
	footer := buildStraight(cfgraph, diamond2, 1)
	buildConnect(cfgraph, diamond2, d11)
	buildConnect(cfgraph, diamond1, header)

	buildConnect(cfgraph, footer, from)
	footer = buildStraight(cfgraph, footer, 1)
	return footer
}

 func buildNestedLoop(cfgraph *cfg.CFG, from int) int {
    outerHeader := buildStraight(cfgraph, from, 1);
    innerHeader := buildStraight(cfgraph, outerHeader, 1);
    innerBody := buildStraight(cfgraph, innerHeader, 2);
    buildConnect(cfgraph, innerBody, innerHeader); // inner back edge
    outerTail := buildStraight(cfgraph, innerBody, 1);
    buildConnect(cfgraph, outerTail, outerHeader); // outer back edge
    return buildStraight(cfgraph, outerTail, 1);
}

func buildMultipleExitLoop(cfgraph *cfg.CFG, from int) int {
    header := buildStraight(cfgraph, from, 1);
    ifNode := buildDiamond(cfgraph, header);

    // first exit path
    exit1 := buildStraight(cfgraph, ifNode, 1);

    // second path with back edge
    path2 := buildStraight(cfgraph, ifNode, 2);
    buildConnect(cfgraph, path2, header); // Back edge

    // connect exits
    merge := buildStraight(cfgraph, exit1, 1);
    buildConnect(cfgraph, path2, merge);

    return merge;
}

func buildSequentialLoops(cfgraph *cfg.CFG, from int) int {
    loop1 := buildBaseLoop(cfgraph, from);
    loop2 := buildBaseLoop(cfgraph, loop1);
    return loop2;
}

func buildLoopWithBranches(cfgraph *cfg.CFG, from int) int {
    header := buildStraight(cfgraph, from, 1);
    branch := buildDiamond(cfgraph, header);
    path1 := buildStraight(cfgraph, branch, 2);
    path2 := buildDiamond(cfgraph, branch);
    merge := buildStraight(cfgraph, path1, 1);
    buildConnect(cfgraph, path2, merge);
    buildConnect(cfgraph, merge, header); // back edge
    return buildStraight(cfgraph, merge, 1);
}

// SCCs with different structures
func createVariedSCC(cfgraph *cfg.CFG, from int, graphType int) int {
    switch (graphType % 5) {
    case 0:
        return buildBaseLoop(cfgraph, from);
    case 1:
        return buildNestedLoop(cfgraph, from);
    case 2:
        return buildMultipleExitLoop(cfgraph, from);
    case 3:
        return buildSequentialLoops(cfgraph, from);
    case 4:
        return buildLoopWithBranches(cfgraph, from);
    }
    return buildBaseLoop(cfgraph, from); // Default case
}

// build a CFG with exactly numSCCs strongly connected components
func buildScalableSCCs(cfgraph *cfg.CFG, numSCCs int) int {
	cfgraph.CreateNode(0);
    current := 0;

    for i := 0; i < numSCCs; i++ {
        // create a varied SCC based on index (for variety)
        current = createVariedSCC(cfgraph, current, i);

        // ensure SCCs are independent by adding a gap node
        if (i < numSCCs - 1) {
            next := current + 1;
            cfgraph.CreateNode(next);
            buildConnect(cfgraph, current, next);
            current = next;
        }
    }

    return current;
}

// run tests with SCC counts 32-8192
func runScalingSCCTests() {
    fmt.Println("=== Testing Scalable SCC Counts ===");

    // Test several different SCC counts
	testCounts := []int{32, 512, 2048, 8192, 16384}
	// testCounts := []int{32}

    for _, count := range testCounts {
		fmt.Printf("Testing with %d SCCs...", count)

        cfgraph := cfg.NewCFG()
        buildScalableSCCs(cfgraph, count);
		fmt.Printf("build complete\n")

        // Test with FWBW algorithm
		lsg1 := lsg.NewLSG()
		start := time.Now()
        loops := fwbwtrim_loopfinder.FindFWBWLoops(cfgraph, lsg1);
		duration := time.Since(start)

		fmt.Printf("FWBW found %d loops (expected %d) in %d ms\n", loops, count + 1, duration.Milliseconds())
		lsg1.CalculateNestingLevel()
		// lsg1.Dump()

		lsg2 := lsg.NewLSG()
		start = time.Now()
        loops = tarjan.FindTarjanLoops(cfgraph, lsg2);
		duration = time.Since(start)

		fmt.Printf("Tarjan found %d loops in %d ms\n", loops, duration.Milliseconds())
		// lsg2.Dump()
    }
}

func main() {
	fmt.Printf("Welcome to LoopTesterApp, Go edition\n")

	cfgraph := cfg.NewCFG()
    fmt.Printf("Constructing Simple CFG...\n");
    cfgraph.CreateNode(0); // top
    buildBaseLoop(cfgraph, 0);
    cfgraph.CreateNode(1); // bottom
    cfg.NewBasicBlockEdge(cfgraph, 0, 2);

    // =========== DUMMY LOOPS TEST FOR BOTH ALGORITHMS ===========
    fmt.Printf("15000 dummy loops for both algorithms\n");

    fwbw_start := time.Now();
    for dummyloops := 0; dummyloops < 1; dummyloops++ {
        fwbwtrim_loopfinder.FindFWBWLoops(cfgraph, lsg.NewLSG());
    }
    fwbw_duration := time.Since(fwbw_start);

    tarjan_start := time.Now();
    for dummyloops := 0; dummyloops < 1; dummyloops++ {
        tarjan.FindTarjanLoops(cfgraph, lsg.NewLSG());
    }
    tarjan_duration := time.Since(tarjan_start)

    // Print timing comparison
    fmt.Printf("Dummy loop times:\n");
    fmt.Printf("  FWBW:   %d milliseconds\n", fwbw_duration.Milliseconds()); // need to divide by 15000
    fmt.Printf("  Tarjan: %d milliseconds\n", tarjan_duration.Milliseconds());

    // =========== BUILD COMPLEX CFG ===========
    fmt.Printf("Constructing complex CFG...\n");
    n := 2

    for parlooptrees := 0; parlooptrees < 10; parlooptrees++ {
        cfgraph.CreateNode(n + 1);
        buildConnect(cfgraph, 2, n + 1);
        n = n + 1;

        for i := 0; i < 100; i++ {
            top := n
            n = buildStraight(cfgraph, n, 1);
            for j := 0; j < 25; j++ {
                n = buildBaseLoop(cfgraph, n);
            }
            bottom := buildStraight(cfgraph, n, 1);
            buildConnect(cfgraph, n, top);
            n = bottom;
        }
        buildConnect(cfgraph, n, 1);
    }

    // =========== SINGLE ITERATION TEST FOR BOTH ALGORITHMS ===========
    fmt.Printf("Performing Loop Recognition\n1 Iteration with both algorithms\n");

    // test each algorithm for a single iteration
	lsg_fwbw := lsg.NewLSG()
	lsg_tarjan := lsg.NewLSG()

    single_fwbw_start := time.Now()
    num_loops_fwbw := fwbwtrim_loopfinder.FindFWBWLoops(cfgraph, lsg_fwbw);
	single_fwbw_duration := time.Since(single_fwbw_start)

    single_tarjan_start := time.Now()
    num_loops_tarjan := tarjan.FindTarjanLoops(cfgraph, lsg_tarjan);
    single_tarjan_duration := time.Since(single_tarjan_start)

    fmt.Printf("Single iteration times:\n")
    fmt.Printf("  FWBW:   %d milliseconds, found %d loops\n", single_fwbw_duration.Milliseconds(), num_loops_fwbw)
    fmt.Printf("  Tarjan: %d milliseconds, found %d loops\n", single_tarjan_duration.Milliseconds(), num_loops_tarjan)


	// =========== SCALING TESTS ===========
    runScalingSCCTests();
    // =========== COMPARISON TESTS ===========
    // compareAllAlgorithms(32);
    // compareAllAlgorithms(512);
    // compareAllAlgorithms(2048);
    // compareAllAlgorithms(8192);
    // compareAllAlgorithms(16384);



	// OLD CODE
	// cfgraph := cfg.NewCFG()

	// fmt.Printf("Constructing Simple CFG...\n")

	// cfgraph.CreateNode(0) // top
	// buildBaseLoop(cfgraph, 0)
	// cfgraph.CreateNode(1) // bottom
	// cfg.NewBasicBlockEdge(cfgraph, 0, 2)

	// fmt.Printf("15000 dummy loops\n")

	// dummyStart := time.Now()
	// for dummyloop := 0; dummyloop < 15000; dummyloop++ {
	// 	havlakloopfinder.FindHavlakLoops(cfgraph, lsg.NewLSG())
	// }
	// dummyElapsed := time.Since(dummyStart)
	// fmt.Printf("Dummy loop time: %d milliseconds\n", dummyElapsed.Milliseconds()) // need to divide by 15000

	// lsgraphDummy := lsg.NewLSG()
	// havlakloopfinder.FindHavlakLoops(cfgraph, lsgraphDummy)
	// fmt.Printf("# of loops in dummy graph: %d (including 1 artificial root node)\n", lsgraphDummy.NumLoops())

	// lsgraphDummy.CalculateNestingLevel()
	// lsgraphDummy.Dump()

	// lsgraph_fwbw := lsg.NewLSG()
	// fwbwtrim_loopfinder.FindFWBWLoops(cfgraph, lsgraph_fwbw)
	// fmt.Printf("# of loops in dummy graph: %d (including 1 artificial root node)\n", lsgraph_fwbw.NumLoops())

	// lsgraph_fwbw.CalculateNestingLevel()
	// lsgraph_fwbw.Dump()




	// OLD CODE
	// fmt.Printf("Constructing CFG...\n")
	// n := 2

	// for parlooptrees := 0; parlooptrees < 10; parlooptrees++ {
	// 	cfgraph.CreateNode(n + 1)
	// 	buildConnect(cfgraph, 2, n+1)
	// 	n = n + 1

	// 	for i := 0; i < 100; i++ {
	// 		top := n
	// 		n = buildStraight(cfgraph, n, 1)
	// 		for j := 0; j < 25; j++ {
	// 			n = buildBaseLoop(cfgraph, n)
	// 		}
	// 		bottom := buildStraight(cfgraph, n, 1)
	// 		buildConnect(cfgraph, n, top)
	// 		n = bottom
	// 	}
	// 	buildConnect(cfgraph, n, 1)
	// }

	// fmt.Printf("Performing Loop Recognition\n1 Iteration\n")
	// havlakloopfinder.FindHavlakLoops(cfgraph, lsgraph)

	// fmt.Printf("Another 50 iterations... (paused for now)\n")

	// complexStart := time.Now()
	// for i := 0; i < 50; i++ {
	// 	// fmt.Printf(".") // don't include in timing
	// 	havlakloopfinder.FindHavlakLoops(cfgraph, lsg.NewLSG())
	// }
	// complexElapsed := time.Since(complexStart)
	// fmt.Printf("Complex loop time: %d milliseconds\n", complexElapsed.Milliseconds()) // need to divide by 50

	// fmt.Printf("\n")
	// fmt.Printf("# of loops: %d (including 1 artificial root node)\n", lsgraph.NumLoops())
	// lsgraph.CalculateNestingLevel()

	// fmt.Printf("\n")
	// fmt.Printf("Now testing FWBW trim algorithm...\n")
	// lsgraph2 := lsg.NewLSG()
	// fwbwtrim_loopfinder.FindFWBWLoops(cfgraph, lsgraph2)
	// fmt.Printf("# of loops: %d (including 1 artificial root node)\n", lsgraph2.NumLoops())


}
