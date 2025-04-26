#include <algorithm>
#include <list>
#include <set>
#include <stdio.h>
#include <vector>

#include "mao-loops.h"
#include "tarjan-loops.h"

// Tarjan's algorithm for finding Strongly Connected Components (loops)
class TarjanLoopFinder {
public:
    TarjanLoopFinder(MaoCFG *cfg, LoopStructureGraph *lsg) : CFG_(cfg), lsg_(lsg), index_(0) {}

    void FindLoops() {
        if (!CFG_->GetStartBasicBlock())
            return;

        // all unvisited
        for (MaoCFG::NodeMap::iterator bb_iter = CFG_->GetBasicBlocks()->begin();
             bb_iter != CFG_->GetBasicBlocks()->end(); ++bb_iter) {
            BasicBlock *bb = (*bb_iter).second;
            if (bb) {
                disc_[bb] = -1;
                low_[bb] = -1;
                on_stack_[bb] = false;
            }
        }

        // DFS from the start node
        StrongConnect(CFG_->GetStartBasicBlock());

        // all loops are found, calculate nesting levels
        lsg_->CalculateNestingLevel();
    }

private:
    void StrongConnect(BasicBlock *node) {
        // depth index of node
        disc_[node] = index_;
        low_[node] = index_;
        index_++;
        stack_.push_back(node);
        on_stack_[node] = true;

        // adjacent nodes
        for (BasicBlock::EdgeVector::iterator it = node->out_edges()->begin();
             it != node->out_edges()->end(); ++it) {
            BasicBlock *w = *it;

            if (disc_[w] == -1) {
                // neighbor unvisited
                StrongConnect(w);
                low_[node] = std::min(low_[node], low_[w]);
            } else if (on_stack_[w]) {
                // neighbor in stack -> in the current SCC
                low_[node] = std::min(low_[node], disc_[w]);
            }
        }

        // if node is a root node, pop the stack and create an SCC
        if (low_[node] == disc_[node]) {
            std::vector<BasicBlock *> component;
            BasicBlock *w;
            do {
                w = stack_.back();
                stack_.pop_back();
                on_stack_[w] = false;
                component.push_back(w);
            } while (w != node);

            // process SCCs with more than one node or self-loops
            bool is_loop = component.size() > 1;
            if (!is_loop) {
                // self-loop
                for (BasicBlock::EdgeVector::iterator it = node->out_edges()->begin();
                     it != node->out_edges()->end(); ++it) {
                    if (*it == node) {
                        is_loop = true;
                        break;
                    }
                }
            }

            if (is_loop) {
                // loop header (entry point to the SCC)
                BasicBlock *header = FindLoopHeader(component);

                // new loop
                SimpleLoop *loop = lsg_->CreateNewLoop();

                // add all nodes from this component
                for (std::vector<BasicBlock *>::iterator it = component.begin();
                     it != component.end(); ++it) {
                    loop->AddNode(*it);
                    node_loop_map_[*it] = loop;
                }

                // add to global loop structure
                lsg_->AddLoop(loop);
            }
        }
    }

    // entry point (header) of an SCC
    BasicBlock *FindLoopHeader(const std::vector<BasicBlock *> &component) {
        std::set<BasicBlock *> component_set(component.begin(), component.end());

        // header is the node with incoming edges from outside the SCC
        for (std::vector<BasicBlock *>::const_iterator it = component.begin();
             it != component.end(); ++it) {
            BasicBlock *node = *it;

            for (BasicBlock::EdgeVector::iterator pred_it = node->in_edges()->begin();
                 pred_it != node->in_edges()->end(); ++pred_it) {
                BasicBlock *pred = *pred_it;

                if (component_set.find(pred) == component_set.end()) {
                    // found incoming edge from outside the SCC
                    return node;
                }
            }
        }

        // no external edges, just use the first node
        return component[0];
    }

    MaoCFG *CFG_;                                        // current control flow graph
    LoopStructureGraph *lsg_;                            // loop forest
    int index_;                                          // discovery time counter
    std::map<BasicBlock *, int> disc_;                   // discovery times
    std::map<BasicBlock *, int> low_;                    // lowlink values
    std::map<BasicBlock *, bool> on_stack_;              // is node on stack?
    std::vector<BasicBlock *> stack_;                    // stack of nodes
    std::map<BasicBlock *, SimpleLoop *> node_loop_map_; // map nodes to their loops
};

int FindTarjanLoops(MaoCFG *CFG, LoopStructureGraph *LSG) {
    TarjanLoopFinder finder(CFG, LSG);
    finder.FindLoops();
    return LSG->GetNumLoops();
}
