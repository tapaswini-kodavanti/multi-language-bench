#include <algorithm>
#include <list>
#include <map>
#include <pthread.h>
#include <set>
#include <stdio.h>
#include <vector>

#include "fwbw-loops.h"
#include "mao-loops.h"

// struct to hold recursive task arguments
struct FindLoopsTask {
    std::set<int> nodeIds;
    class FWBWLoopFinder *finder;
};

// parallel Forward-Backward Trim algorithm for finding loops
class FWBWLoopFinder {
public:
    FWBWLoopFinder(MaoCFG *cfg, LoopStructureGraph *lsg)
        : CFG_(cfg), lsg_(lsg), taskCount_(0) {
        pthread_mutex_init(&lsgMutex_, nullptr);
        pthread_mutex_init(&nodeLoopMapMutex_, nullptr);
        pthread_mutex_init(&taskCountMutex_, nullptr);
        pthread_cond_init(&taskCompleteCond_, nullptr);
    }

    ~FWBWLoopFinder() {
        pthread_mutex_destroy(&lsgMutex_);
        pthread_mutex_destroy(&nodeLoopMapMutex_);
        pthread_mutex_destroy(&taskCountMutex_);
        pthread_cond_destroy(&taskCompleteCond_);
    }

    void FindLoops() {
        if (!CFG_->GetStartBasicBlock())
            return;

        for (MaoCFG::NodeMap::iterator bb_iter = CFG_->GetBasicBlocks()->begin();
             bb_iter != CFG_->GetBasicBlocks()->end(); ++bb_iter) {
            BasicBlock *bb = (*bb_iter).second;
            if (bb) {
                nodeToId_[bb] = bb_iter->first;
                idToNode_[bb_iter->first] = bb;
            }
        }

        std::set<int> workingSet;
        for (auto &pair : idToNode_) {
            workingSet.insert(pair.first);
        }

        FindLoopsRecursive(workingSet);

        // barrier
        waitForTasks();

        lsg_->CalculateNestingLevel();
    }

    // thread function to process a partition
    static void *threadFunction(void *arg) {
        FindLoopsTask *task = static_cast<FindLoopsTask *>(arg);
        FWBWLoopFinder *finder = task->finder;

        finder->FindLoopsRecursive(task->nodeIds);

        pthread_mutex_lock(&finder->taskCountMutex_);
        finder->taskCount_--;
        pthread_cond_signal(&finder->taskCompleteCond_);
        pthread_mutex_unlock(&finder->taskCountMutex_);

        delete task;
        return nullptr;
    }

    void FindLoopsRecursive(const std::set<int> &nodeIds) {
        // base case: if 1 vertex or less, return (no more loops)
        if (nodeIds.size() <= 1)
            return;

        // apply forward trim
        std::set<int> remaining = TrimForward(nodeIds);
        if (remaining.empty())
            return;

        // apply backward trim
        remaining = TrimBackward(remaining);
        if (remaining.empty())
            return;

        // pick a pivot node
        int pivotId = *remaining.begin();
        BasicBlock *pivot = idToNode_[pivotId];

        // find nodes reachable from pivot (descendants)
        std::set<int> desc = Reachable(pivot, remaining, true);

        // find nodes that can reach pivot (predecessors)
        std::set<int> pred = Reachable(pivot, remaining, false);

        // compute intersection to find SCC
        std::set<int> scc = Intersect(pred, desc);

        // compute the three partitions for recursive processing
        std::set<int> predMinusSCC = Difference(pred, scc);
        std::set<int> descMinusSCC = Difference(desc, scc);

        // compute remaining - (pred ∪ desc)
        std::set<int> predDesc = Union(pred, desc);
        std::set<int> rem = Difference(remaining, predDesc);

        // thread threshold
        const int PARALLEL_THRESHOLD = 50;

        // launch threads for non-empty partitions that exceed the threshold
        if (predMinusSCC.size() > PARALLEL_THRESHOLD) {
            launchThread(predMinusSCC);
        } else if (!predMinusSCC.empty()) {
            FindLoopsRecursive(predMinusSCC);
        }

        if (descMinusSCC.size() > PARALLEL_THRESHOLD) {
            launchThread(descMinusSCC);
        } else if (!descMinusSCC.empty()) {
            FindLoopsRecursive(descMinusSCC);
        }

        if (rem.size() > PARALLEL_THRESHOLD) {
            launchThread(rem);
        } else if (!rem.empty()) {
            FindLoopsRecursive(rem);
        }

        // process the SCC if it's a valid loop
        if (!scc.empty()) {
            pthread_mutex_lock(&lsgMutex_);
            SimpleLoop *loop = lsg_->CreateNewLoop();
            pthread_mutex_unlock(&lsgMutex_);

            // find loop header (entry point)
            BasicBlock *header = FindLoopHeader(scc);

            // add nodes to the loop
            for (int id : scc) {
                BasicBlock *bb = idToNode_[id];

                pthread_mutex_lock(&nodeLoopMapMutex_);
                // check if this node is already in another loop
                if (nodeLoopMap_.find(bb) != nodeLoopMap_.end()) {
                    // handle nesting
                    SimpleLoop *innerLoop = nodeLoopMap_[bb];
                    if (innerLoop != loop) {
                        innerLoop->set_parent(loop);
                    }
                } else {
                    // add to loop
                    loop->AddNode(bb);
                    nodeLoopMap_[bb] = loop;
                }
                pthread_mutex_unlock(&nodeLoopMapMutex_);
            }

            // add to global loop structure
            pthread_mutex_lock(&lsgMutex_);
            lsg_->AddLoop(loop);
            pthread_mutex_unlock(&lsgMutex_);
        }
    }

    void launchThread(const std::set<int> &nodeIds) {
        pthread_t thread;
        FindLoopsTask *task = new FindLoopsTask{nodeIds, this};

        pthread_mutex_lock(&taskCountMutex_);
        taskCount_++;
        pthread_mutex_unlock(&taskCountMutex_);

        pthread_create(&thread, nullptr, threadFunction, task);
        pthread_detach(thread); // wait using condition variables
    }

    void waitForTasks() {
        pthread_mutex_lock(&taskCountMutex_);
        while (taskCount_ > 0) {
            pthread_cond_wait(&taskCompleteCond_, &taskCountMutex_);
        }
        pthread_mutex_unlock(&taskCountMutex_);
    }

    std::set<int> TrimForward(const std::set<int> &nodeIds) {
        std::set<int> result = nodeIds;
        bool modified;

        do {
            modified = false;
            std::vector<int> toRemove;

            for (int id : result) {
                BasicBlock *bb = idToNode_[id];
                bool hasPredInSet = false;

                for (BasicBlock::EdgeVector::iterator it = bb->in_edges()->begin();
                     it != bb->in_edges()->end(); ++it) {
                    BasicBlock *pred = *it;
                    if (result.find(nodeToId_[pred]) != result.end()) {
                        hasPredInSet = true;
                        break;
                    }
                }

                if (!hasPredInSet) {
                    toRemove.push_back(id);
                    modified = true;
                }
            }

            for (int id : toRemove) {
                result.erase(id);
            }
        } while (modified);

        return result;
    }

    std::set<int> TrimBackward(const std::set<int> &nodeIds) {
        std::set<int> result = nodeIds;
        bool modified;

        do {
            modified = false;
            std::vector<int> toRemove;

            for (int id : result) {
                BasicBlock *bb = idToNode_[id];
                bool hasSuccInSet = false;

                for (BasicBlock::EdgeVector::iterator it = bb->out_edges()->begin();
                     it != bb->out_edges()->end(); ++it) {
                    BasicBlock *succ = *it;
                    if (result.find(nodeToId_[succ]) != result.end()) {
                        hasSuccInSet = true;
                        break;
                    }
                }

                if (!hasSuccInSet) {
                    toRemove.push_back(id);
                    modified = true;
                }
            }

            for (int id : toRemove) {
                result.erase(id);
            }
        } while (modified);

        return result;
    }

    std::set<int> Reachable(BasicBlock *start, const std::set<int> &nodeIds, bool forward) {
        std::set<int> result;
        std::set<int> visited;
        std::vector<BasicBlock *> stack;

        stack.push_back(start);

        while (!stack.empty()) {
            BasicBlock *node = stack.back();
            stack.pop_back();

            int nodeId = nodeToId_[node];
            if (visited.find(nodeId) != visited.end())
                continue;

            visited.insert(nodeId);
            if (nodeIds.find(nodeId) != nodeIds.end()) {
                result.insert(nodeId);
            }

            // add neighbors to stack
            BasicBlock::EdgeVector &edges = forward ? *node->out_edges() : *node->in_edges();
            for (BasicBlock::EdgeVector::iterator it = edges.begin(); it != edges.end(); ++it) {
                BasicBlock *neighbor = *it;
                int neighborId = nodeToId_[neighbor];

                if (nodeIds.find(neighborId) != nodeIds.end()) {
                    stack.push_back(neighbor);
                }
            }
        }

        return result;
    }

    std::set<int> Intersect(const std::set<int> &a, const std::set<int> &b) {
        std::set<int> result;

        for (int id : a) {
            if (b.find(id) != b.end()) {
                result.insert(id);
            }
        }

        return result;
    }

    std::set<int> Union(const std::set<int> &a, const std::set<int> &b) {
        std::set<int> result = a;

        for (int id : b) {
            result.insert(id);
        }

        return result;
    }

    std::set<int> Difference(const std::set<int> &a, const std::set<int> &b) {
        std::set<int> result;

        for (int id : a) {
            if (b.find(id) == b.end()) {
                result.insert(id);
            }
        }

        return result;
    }

    BasicBlock *FindLoopHeader(const std::set<int> &scc) {
        // header is a node with incoming edges from outside the SCC
        for (int id : scc) {
            BasicBlock *node = idToNode_[id];

            for (BasicBlock::EdgeVector::iterator it = node->in_edges()->begin();
                 it != node->in_edges()->end(); ++it) {
                BasicBlock *pred = *it;
                if (scc.find(nodeToId_[pred]) == scc.end()) {
                    return node; // found a node with an edge from outside the SCC
                }
            }
        }

        // if no external edges, just use the first node
        return idToNode_[*scc.begin()];
    }

    MaoCFG *CFG_;                                      // current control flow graph
    LoopStructureGraph *lsg_;                          // loop forest
    std::map<BasicBlock *, SimpleLoop *> nodeLoopMap_; // map nodes to their loops
    std::map<BasicBlock *, int> nodeToId_;             // map from nodes to IDs
    std::map<int, BasicBlock *> idToNode_;             // map from IDs to nodes

    // synchronization primitives
    pthread_mutex_t lsgMutex_;         // protects access to the loop structure graph
    pthread_mutex_t nodeLoopMapMutex_; // protects access to the node-to-loop mapping
    pthread_mutex_t taskCountMutex_;   // protects the task counter
    pthread_cond_t taskCompleteCond_;  // condition variable for task completion
    int taskCount_;                    // counter for outstanding tasks
};

// external entry point for FWBW Trim algorithm
int FindFWBWLoops(MaoCFG *CFG, LoopStructureGraph *LSG) {
    FWBWLoopFinder finder(CFG, LSG);
    finder.FindLoops();
    return LSG->GetNumLoops();
}