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

#include <algorithm>
#include <chrono>
#include <list>
#include <map>
#include <set>
#include <stdio.h>
#include <vector>

#include "fwbw-loops.h"
#include "mao-loops.h"
#include "tarjan-loops.h"

using namespace std;

int buildDiamond(MaoCFG *cfg, int start) {
    int bb0 = start;

    new BasicBlockEdge(cfg, bb0, bb0 + 1);
    new BasicBlockEdge(cfg, bb0, bb0 + 2);
    new BasicBlockEdge(cfg, bb0 + 1, bb0 + 3);
    new BasicBlockEdge(cfg, bb0 + 2, bb0 + 3);

    return bb0 + 3;
}

void buildConnect(MaoCFG *cfg, int start, int end) {
    new BasicBlockEdge(cfg, start, end);
}

int buildStraight(MaoCFG *cfg, int start, int n) {
    for (int i = 0; i < n; i++) {
        buildConnect(cfg, start + i, start + i + 1);
    }

    return start + n;
}

int buildBaseLoop(MaoCFG *cfg, int from) {
    int header = buildStraight(cfg, from, 1);
    int diamond1 = buildDiamond(cfg, header);
    int d11 = buildStraight(cfg, diamond1, 1);
    int diamond2 = buildDiamond(cfg, d11);
    int footer = buildStraight(cfg, diamond2, 1);
    buildConnect(cfg, diamond2, d11);
    buildConnect(cfg, diamond1, header);

    buildConnect(cfg, footer, from);
    footer = buildStraight(cfg, footer, 1);
    return footer;
}

/*
for (int i = 0; i < n; i++) {     // outer loop
    for (int j = 0; j < m; j++) { // inner loop
        // inner body (2 nodes)
    }
    // outer tail
}
// exit node
 */
int buildNestedLoop(MaoCFG *cfg, int from) {
    int outerHeader = buildStraight(cfg, from, 1);
    int innerHeader = buildStraight(cfg, outerHeader, 1);
    int innerBody = buildStraight(cfg, innerHeader, 2);
    buildConnect(cfg, innerBody, innerHeader); // inner back edge
    int outerTail = buildStraight(cfg, innerBody, 1);
    buildConnect(cfg, outerTail, outerHeader); // outer back edge
    return buildStraight(cfg, outerTail, 1);
}

/*
while (true) {                // header
    if (condition) {          // diamond
        break;                // exit1
    } else {
        // loop body (path2)
        if (another_condition)
            continue;         // back edge to header
        else
            break;            // exit to merge
    }
}
// merge point
*/
int buildMultipleExitLoop(MaoCFG *cfg, int from) {
    int header = buildStraight(cfg, from, 1);
    int ifNode = buildDiamond(cfg, header);

    // first exit path
    int exit1 = buildStraight(cfg, ifNode, 1);

    // second path with back edge
    int path2 = buildStraight(cfg, ifNode, 2);
    buildConnect(cfg, path2, header); // Back edge

    // connect exits
    int merge = buildStraight(cfg, exit1, 1);
    buildConnect(cfg, path2, merge);

    return merge;
}

/*
for (int i = 0; i < n; i++) {
    // first loop with complex diamond control flow
}
for (int j = 0; j < m; j++) {
    // second loop with complex diamond control flow
}
*/
int buildSequentialLoops(MaoCFG *cfg, int from) {
    int loop1 = buildBaseLoop(cfg, from);
    int loop2 = buildBaseLoop(cfg, loop1);
    return loop2;
}

/*
while (true) {                // header
    if (condition1) {         // first diamond
        // path1 - two straight blocks
    } else {
        if (condition2) {     // second diamond (path2)
            // then path
        } else {
            // else path
        }
    }
    // merge point
    if (exit_condition)
        break;                // to exit node
    // else continue loop (back edge)
}
// exit node
*/
int buildLoopWithBranches(MaoCFG *cfg, int from) {
    int header = buildStraight(cfg, from, 1);
    int branch = buildDiamond(cfg, header);
    int path1 = buildStraight(cfg, branch, 2);
    int path2 = buildDiamond(cfg, branch);
    int merge = buildStraight(cfg, path1, 1);
    buildConnect(cfg, path2, merge);
    buildConnect(cfg, merge, header); // back edge
    return buildStraight(cfg, merge, 1);
}

// SCCs with different structures
int createVariedSCC(MaoCFG *cfg, int from, int type) {
    switch (type % 5) {
    case 0:
        return buildBaseLoop(cfg, from);
    case 1:
        return buildNestedLoop(cfg, from);
    case 2:
        return buildMultipleExitLoop(cfg, from);
    case 3:
        return buildSequentialLoops(cfg, from);
    case 4:
        return buildLoopWithBranches(cfg, from);
    }
    return buildBaseLoop(cfg, from); // Default case
}

// build a CFG with exactly numSCCs strongly connected components
int buildScalableSCCs(MaoCFG *cfg, int numSCCs) {
    cfg->CreateNode(0);
    int current = 0;

    for (int i = 0; i < numSCCs; i++) {
        // create a varied SCC based on index (for variety)
        current = createVariedSCC(cfg, current, i);

        // ensure SCCs are independent by adding a gap node
        if (i < numSCCs - 1) {
            int next = current + 1;
            cfg->CreateNode(next);
            buildConnect(cfg, current, next);
            current = next;
        }
    }

    return current;
}

// run tests with SCC counts 32-8192
void runScalingSCCTests() {
    fprintf(stderr, "\n=== Testing Scalable SCC Counts ===\n");

    // Test several different SCC counts
    int testCounts[] = {32, 512, 2048/*, 8192, 16384*/};

    for (int count : testCounts) {
        fprintf(stderr, "\nTesting with %d SCCs...\n", count);

        MaoCFG cfg;
        buildScalableSCCs(&cfg, count);

        // Test with FWBW algorithm
        LoopStructureGraph lsg;
        auto start = chrono::high_resolution_clock::now();
        int loops = FindFWBWLoops(&cfg, &lsg);
        auto end = chrono::high_resolution_clock::now();

        chrono::duration<double, milli> duration = end - start;

        fprintf(stderr, "FWBW found %d loops (expected %d) in %.2f ms\n",
                loops, count + 1, duration.count()); // +1 for artificial root

        LoopStructureGraph lsg2;
        start = chrono::high_resolution_clock::now();
        loops = FindTarjanLoops(&cfg, &lsg2);
        end = chrono::high_resolution_clock::now();

        fprintf(stderr, "Tarjan found %d loops in %.2f ms\n",
                loops, chrono::duration<double, milli>(end - start).count());
    }
}

// compare all algorithms with a given number of SCCs
void compareAllAlgorithms(int count) {
    fprintf(stderr, "\n=== Comparing All Algorithms with %d SCCs ===\n", count);

    MaoCFG cfg;
    buildScalableSCCs(&cfg, count);

    // test Tarjan
    LoopStructureGraph lsg1;
    auto start = chrono::high_resolution_clock::now();
    int loops = FindTarjanLoops(&cfg, &lsg1);
    auto end = chrono::high_resolution_clock::now();
    fprintf(stderr, "Tarjan: %d loops in %.2f ms\n",
            loops, chrono::duration<double, milli>(end - start).count());

    // test FWBW
    LoopStructureGraph lsg2;
    start = chrono::high_resolution_clock::now();
    loops = FindFWBWLoops(&cfg, &lsg2);
    end = chrono::high_resolution_clock::now();
    fprintf(stderr, "FWBW: %d loops in %.2f ms\n",
            loops, chrono::duration<double, milli>(end - start).count());
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////MAIN FUNCTION BELOW//////////////////////////////////

int main(int argc, char *argv[]) {
    fprintf(stderr, "Welcome to LoopTesterApp, C++ edition\n");
    fprintf(stderr, "Constructing cfg...\n");
    MaoCFG cfg;
    fprintf(stderr, "Constructing lsg...\n");
    LoopStructureGraph lsg;

    fprintf(stderr, "Constructing Simple CFG...\n");
    cfg.CreateNode(0); // top
    buildBaseLoop(&cfg, 0);
    cfg.CreateNode(1); // bottom
    new BasicBlockEdge(&cfg, 0, 2);

    // =========== DUMMY LOOPS TEST FOR BOTH ALGORITHMS ===========
    fprintf(stderr, "15000 dummy loops for both algorithms\n");

    vector<LoopStructureGraph *> to_delete;
    auto fwbw_start = chrono::high_resolution_clock::now();
    for (int dummyloops = 0; dummyloops < 15000; ++dummyloops) {
        LoopStructureGraph *lsglocal = new LoopStructureGraph();
        FindFWBWLoops(&cfg, lsglocal);
        to_delete.push_back(lsglocal);
    }
    auto fwbw_end = chrono::high_resolution_clock::now();

    auto tarjan_start = chrono::high_resolution_clock::now();
    for (int dummyloops = 0; dummyloops < 15000; ++dummyloops) {
        LoopStructureGraph *lsglocal = new LoopStructureGraph();
        FindTarjanLoops(&cfg, lsglocal);
        to_delete.push_back(lsglocal);
    }
    auto tarjan_end = chrono::high_resolution_clock::now();

    // clean up!
    for (auto p : to_delete) {
        delete (p);
    }

    // Print timing comparison
    chrono::duration<double, milli> fwbw_duration = fwbw_end - fwbw_start;
    chrono::duration<double, milli> tarjan_duration = tarjan_end - tarjan_start;

    fprintf(stderr, "Dummy loop times per iteration:\n");
    fprintf(stderr, "  FWBW:   %f milliseconds\n", fwbw_duration.count() / 15000);
    fprintf(stderr, "  Tarjan: %f milliseconds\n", tarjan_duration.count() / 15000);

    // =========== BUILD COMPLEX CFG ===========
    fprintf(stderr, "Constructing complex CFG...\n");
    int n = 2;

    for (int parlooptrees = 0; parlooptrees < 10; parlooptrees++) {
        cfg.CreateNode(n + 1);
        buildConnect(&cfg, 2, n + 1);
        n = n + 1;

        for (int i = 0; i < 100; i++) {
            int top = n;
            n = buildStraight(&cfg, n, 1);
            for (int j = 0; j < 25; j++) {
                n = buildBaseLoop(&cfg, n);
            }
            int bottom = buildStraight(&cfg, n, 1);
            buildConnect(&cfg, n, top);
            n = bottom;
        }
        buildConnect(&cfg, n, 1);
    }

    // =========== SINGLE ITERATION TEST FOR BOTH ALGORITHMS ===========
    fprintf(stderr, "Performing Loop Recognition\n1 Iteration with both algorithms\n");

    // test each algorithm for a single iteration
    LoopStructureGraph lsg_fwbw, lsg_tarjan;

    auto single_fwbw_start = chrono::high_resolution_clock::now();
    int num_loops_fwbw = FindFWBWLoops(&cfg, &lsg_fwbw);
    auto single_fwbw_end = chrono::high_resolution_clock::now();

    auto single_tarjan_start = chrono::high_resolution_clock::now();
    int num_loops_tarjan = FindTarjanLoops(&cfg, &lsg_tarjan);
    auto single_tarjan_end = chrono::high_resolution_clock::now();

    fprintf(stderr, "Single iteration times:\n");
    fprintf(stderr, "  FWBW:   %f milliseconds, found %d loops\n",
            chrono::duration<double, milli>(single_fwbw_end - single_fwbw_start).count(),
            num_loops_fwbw);
    fprintf(stderr, "  Tarjan: %f milliseconds, found %d loops\n",
            chrono::duration<double, milli>(single_tarjan_end - single_tarjan_start).count(),
            num_loops_tarjan);

    // =========== 50 ITERATIONS TEST FOR BOTH ALGORITHMS ===========
    /*
    fprintf(stderr, "Another 50 iterations with both algorithms...\n");

    int sum_fwbw = 0, sum_tarjan = 0;

    // test FWBW
    auto complex_fwbw_start = chrono::high_resolution_clock::now();
    for (int i = 0; i < 50; i++) {
        LoopStructureGraph lsg;
        sum_fwbw += FindFWBWLoops(&cfg, &lsg);
    }
    auto complex_fwbw_end = chrono::high_resolution_clock::now();

    // test Tarjan
    auto complex_tarjan_start = chrono::high_resolution_clock::now();
    for (int i = 0; i < 50; i++) {
        LoopStructureGraph lsg;
        sum_tarjan += FindTarjanLoops(&cfg, &lsg);
    }
    auto complex_tarjan_end = chrono::high_resolution_clock::now();

    chrono::duration<double, milli> complex_fwbw_duration = complex_fwbw_end - complex_fwbw_start;
    chrono::duration<double, milli> complex_tarjan_duration = complex_tarjan_end - complex_tarjan_start;

    fprintf(stderr, "Complex loop times (average of 50 iterations):\n");
    fprintf(stderr, "  FWBW:   %f milliseconds, total loops: %d\n",
            complex_fwbw_duration.count() / 50, sum_fwbw);
    fprintf(stderr, "  Tarjan: %f milliseconds, total loops: %d\n",
            complex_tarjan_duration.count() / 50, sum_tarjan);

    fprintf(stderr, "\nDetailed loop structures:\n");
    fprintf(stderr, "FWBW:\n");
    lsg_fwbw.Dump();
    fprintf(stderr, "Tarjan:\n");
    lsg_tarjan.Dump();
    */

    // =========== SCALING TESTS ===========
    runScalingSCCTests();
    // =========== COMPARISON TESTS ===========
    // compareAllAlgorithms(32);
    // compareAllAlgorithms(512);
    // compareAllAlgorithms(2048);
    // compareAllAlgorithms(8192);
    // compareAllAlgorithms(16384);

    return 0;
}
