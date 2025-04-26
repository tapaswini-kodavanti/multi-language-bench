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

// just to see if the new build functions work...
void testReducibleGraphs() {
    fprintf(stderr, "\n=== Testing Various Reducible Graph Patterns ===\n\n");

    // test case 1: simple loop
    {
        fprintf(stderr, "Test 1: Simple Loop\n");
        MaoCFG cfg;
        LoopStructureGraph lsg;

        cfg.CreateNode(0);
        buildBaseLoop(&cfg, 0);

        int loops = FindHavlakLoops(&cfg, &lsg);
        fprintf(stderr, "Found %d loops\n\n", loops);
        // lsg.Dump();
    }

    // test case 2: nested loops
    {
        fprintf(stderr, "Test 2: Nested Loops\n");
        MaoCFG cfg;
        LoopStructureGraph lsg;

        cfg.CreateNode(0);
        buildNestedLoop(&cfg, 0);

        int loops = FindHavlakLoops(&cfg, &lsg);
        fprintf(stderr, "Found %d loops\n\n", loops);
        // lsg.Dump();
    }

    // test case 3: multiple exit loop
    {
        fprintf(stderr, "Test 3: Multiple Exit Loop\n");
        MaoCFG cfg;
        LoopStructureGraph lsg;

        cfg.CreateNode(0);
        buildMultipleExitLoop(&cfg, 0);

        int loops = FindHavlakLoops(&cfg, &lsg);
        fprintf(stderr, "Found %d loops\n\n", loops);
        // lsg.Dump();
    }

    // test case 4: sequential loops
    {
        fprintf(stderr, "Test 4: Sequential Loops\n");
        MaoCFG cfg;
        LoopStructureGraph lsg;

        cfg.CreateNode(0);
        buildSequentialLoops(&cfg, 0);

        int loops = FindHavlakLoops(&cfg, &lsg);
        fprintf(stderr, "Found %d loops\n\n", loops);
        // lsg.Dump();
    }

    // test case 5: loop with branches
    {
        fprintf(stderr, "Test 5: Loop with Branches\n");
        MaoCFG cfg;
        LoopStructureGraph lsg;

        cfg.CreateNode(0);
        buildLoopWithBranches(&cfg, 0);

        int loops = FindHavlakLoops(&cfg, &lsg);
        fprintf(stderr, "Found %d loops\n\n", loops);
        // lsg.Dump();
    }

    // test case 6: complex loop with multiple exits
    {
        fprintf(stderr, "Test 6: Complex Loop with Multiple Exits\n");
        MaoCFG cfg;
        LoopStructureGraph lsg;

        cfg.CreateNode(0);
        buildBaseLoop(&cfg, 0);
        buildBaseLoop(&cfg, 1);

        int loops = FindHavlakLoops(&cfg, &lsg);
        fprintf(stderr, "Found %d loops\n\n", loops);
        // lsg.Dump();
    }
}

// create large scale test with above build helpers
void testLargeScaleOne() {
    MaoCFG cfg;
    LoopStructureGraph lsg;

    cfg.CreateNode(0);
    int n = 0;

    for (int i = 0; i < 100; i++) {
        int loopHead = n;
        n = buildStraight(&cfg, n, 1);

        for (int j = 0; j < 10; j++) {
            if (j % 3 == 0)
                n = buildNestedLoop(&cfg, n);
            else if (j % 3 == 1)
                n = buildMultipleExitLoop(&cfg, n);
            else
                n = buildLoopWithBranches(&cfg, n);
        }

        buildConnect(&cfg, n, loopHead);
    }

    fprintf(stderr, "Testing large scale reducible graph one...\n");
    auto start = chrono::high_resolution_clock::now();
    int loops = FindHavlakLoops(&cfg, &lsg);
    auto end = chrono::high_resolution_clock::now();

    chrono::duration<double, milli> duration = end - start;
    fprintf(stderr, "Found %d loops in %f milliseconds\n",
            loops, duration.count());
}

int largeScaleTestTwo(MaoCFG *cfg, int from) {
    cfg->CreateNode(from);
    int current = from;

    // "islands" of loops
    const int NUM_ISLANDS = 50;
    std::vector<int> islandEntries;
    std::vector<int> islandExits;

    for (int i = 0; i < NUM_ISLANDS; i++) {
        // entry point of each
        islandEntries.push_back(current);

        int nestingDepth = 10;
        int nestStart = current;

        int entryDiamond = buildDiamond(cfg, current);
        current = entryDiamond;

        for (int depth = 0; depth < nestingDepth; depth++) {
            if (depth % 3 == 0) {
                // every third level is a complex loop with branches
                current = buildLoopWithBranches(cfg, current);
            } else if (depth % 3 == 1) {
                // every third+1 level is a nested loop
                current = buildNestedLoop(cfg, current);
            } else {
                // every third+2 level is a multiple-exit loop
                current = buildMultipleExitLoop(cfg, current);
            }

            current = buildStraight(cfg, current, 3);

            if (depth % 2 == 0) {
                current = buildDiamond(cfg, current);
            }
        }

        int loopCluster = current;
        for (int j = 0; j < 20; j++) {
            if (j % 4 == 0) {
                current = buildBaseLoop(cfg, current);
            } else if (j % 4 == 1) {
                current = buildNestedLoop(cfg, current);
            } else if (j % 4 == 2) {
                current = buildLoopWithBranches(cfg, current);
            } else {
                current = buildMultipleExitLoop(cfg, current);
            }
        }

        buildConnect(cfg, current, loopCluster + 10);

        current = buildStraight(cfg, current, 1);
        islandExits.push_back(current);

        current = current + 1;
    }

    // connections between islands
    for (int i = 0; i < NUM_ISLANDS; i++) {
        if (i < NUM_ISLANDS - 1) {
            buildConnect(cfg, islandExits[i], islandEntries[i + 1]);
        }

        // cross-connections to create more complex control flow
        // designed to maintain reducibility: connect earlier exits to later
        // entries, never the reverse
        for (int j = i + 2; j < std::min(i + 10, NUM_ISLANDS); j++) {
            buildConnect(cfg, islandExits[i], islandEntries[j]);
        }
    }

    buildConnect(cfg, islandExits[NUM_ISLANDS - 1], islandEntries[0]);

    int finalExit = buildStraight(cfg, islandExits[NUM_ISLANDS - 1], 1);

    return finalExit;
}

void testLargeScaleTwo() {
    fprintf(stderr, "Testing large scale reducible graph two...\n");
    MaoCFG cfg;
    LoopStructureGraph lsg;

    largeScaleTestTwo(&cfg, 0);

    auto start = chrono::high_resolution_clock::now();
    int loops = FindHavlakLoops(&cfg, &lsg);
    auto end = chrono::high_resolution_clock::now();

    chrono::duration<double, milli> duration = end - start;
    fprintf(stderr, "Found %d loops in %f milliseconds\n",
            loops, duration.count());
}

int largeScaleTestThree(MaoCFG *cfg, int from) {
    int maxNodeId = from;

    const int MAIN_PATH_LENGTH = 100;  // length of the main path
    const int SIDE_PATHS_PER_NODE = 2; // number of side paths from each main node
    const int SIDE_PATH_LOOPS = 3;     // number of loops in each side path
    const int NESTING_LEVELS = 15;     // levels of nesting in special deep sections

    // main path with nodes
    std::vector<int> mainPath(MAIN_PATH_LENGTH);
    mainPath[0] = from;

    for (int i = 1; i < MAIN_PATH_LENGTH; i++) {
        mainPath[i] = ++maxNodeId;
        cfg->CreateNode(mainPath[i]);
        buildConnect(cfg, mainPath[i - 1], mainPath[i]);
    }

    for (int i = 0; i < MAIN_PATH_LENGTH; i++) {
        for (int j = 0; j < SIDE_PATHS_PER_NODE; j++) {
            int sideStart = ++maxNodeId;
            cfg->CreateNode(sideStart);
            buildConnect(cfg, mainPath[i], sideStart);

            int current = sideStart;

            for (int k = 0; k < SIDE_PATH_LOOPS; k++) {
                switch ((i + j + k) % 4) {
                case 0:
                    current = buildBaseLoop(cfg, current);
                    break;
                case 1:
                    current = buildNestedLoop(cfg, current);
                    break;
                case 2:
                    current = buildMultipleExitLoop(cfg, current);
                    break;
                case 3:
                    current = buildLoopWithBranches(cfg, current);
                    break;
                }
                maxNodeId = std::max(maxNodeId, current);
            }

            buildConnect(cfg, current, mainPath[std::min(i + 5, MAIN_PATH_LENGTH - 1)]);
        }

        if (i % 10 == 0 && i > 0) {
            int deepStart = ++maxNodeId;
            cfg->CreateNode(deepStart);
            buildConnect(cfg, mainPath[i], deepStart);

            int current = deepStart;
            std::vector<int> nestingHeaders;
            nestingHeaders.push_back(current);

            for (int level = 0; level < NESTING_LEVELS; level++) {
                int levelHeader = ++maxNodeId;
                cfg->CreateNode(levelHeader);
                buildConnect(cfg, current, levelHeader);
                current = levelHeader;
                nestingHeaders.push_back(current);

                for (int l = 0; l < 3; l++) {
                    current = buildBaseLoop(cfg, current);
                    maxNodeId = std::max(maxNodeId, current);
                }
            }

            for (int n = 1; n < nestingHeaders.size(); n++) {
                buildConnect(cfg, current, nestingHeaders[n]);
            }

            buildConnect(cfg, current, mainPath[std::min(i + 5, MAIN_PATH_LENGTH - 1)]);
        }
    }

    for (int i = 0; i < MAIN_PATH_LENGTH - 10; i++) {
        if (i % 7 == 0) {
            buildConnect(cfg, mainPath[i + 9], mainPath[i]);
        }
    }

    int finalNode = ++maxNodeId;
    cfg->CreateNode(finalNode);
    buildConnect(cfg, mainPath[MAIN_PATH_LENGTH - 1], finalNode);

    return finalNode;
}

void testLargeScaleThree() {
    fprintf(stderr, "Testing large scale reducible graph three...\n");
    MaoCFG cfg;
    LoopStructureGraph lsg;

    cfg.CreateNode(0);
    largeScaleTestThree(&cfg, 0);

    auto start = chrono::high_resolution_clock::now();
    int loops = FindHavlakLoops(&cfg, &lsg);
    auto end = chrono::high_resolution_clock::now();

    chrono::duration<double, milli> duration = end - start;
    fprintf(stderr, "Found %d loops in %f milliseconds\n",
            loops, duration.count());
}

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

    fprintf(stderr, "15000 dummy loops\n");

    auto dummy_start = chrono::high_resolution_clock::now();
    vector<LoopStructureGraph *> to_delete;
    for (int dummyloops = 0; dummyloops < 15000; ++dummyloops) {
        LoopStructureGraph *lsglocal = new LoopStructureGraph();
        FindFWBWLoops(&cfg, lsglocal);
        // FindTarjanLoops(&cfg, lsglocal);
        // FindHavlakLoops(&cfg, lsglocal);
        // delete(lsglocal); // don't include this for timing

        to_delete.push_back(lsglocal);
    }
    auto dummy_end = chrono::high_resolution_clock::now();

    for (auto p : to_delete) {
        delete (p);
    }

    chrono::duration<double, milli> dummy_duration = dummy_end - dummy_start;
    fprintf(stderr, "Dummy loop time: %f milliseconds\n", dummy_duration.count() / 15000);

    fprintf(stderr, "Constructing CFG...\n");
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

    fprintf(stderr, "Performing Loop Recognition\n1 Iteration\n");
    int num_loops = FindFWBWLoops(&cfg, &lsg);
    // int num_loops = FindTarjanLoops(&cfg, &lsg);
    // int num_loops = FindHavlakLoops(&cfg, &lsg);

    fprintf(stderr, "Another 50 iterations...\n");

    auto complex_start = chrono::high_resolution_clock::now();
    int sum = 0;
    for (int i = 0; i < 50; i++) {
        LoopStructureGraph lsg;
        // fprintf(stderr, "."); // don't include this for timing
        sum += FindFWBWLoops(&cfg, &lsg);
        // sum += FindTarjanLoops(&cfg, &lsg);
        // sum += FindHavlakLoops(&cfg, &lsg);
    }
    auto complex_end = chrono::high_resolution_clock::now();

    chrono::duration<double, milli> complex_duration = dummy_end - dummy_start;
    fprintf(stderr, "Complex loop time: %f milliseconds\n", complex_duration.count() / 50);

    fprintf(stderr,
            "\nFound %d loops (including artificial root node)"
            "(%d)\n",
            num_loops, sum);
    lsg.Dump();

    // testReducibleGraphs();
    // testLargeScaleOne();
    // testLargeScaleTwo();
    // testLargeScaleThree();
    return 0;
}
