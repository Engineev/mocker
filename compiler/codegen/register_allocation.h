// Iterated Register Coalescing, Lal George and Andrew Appel, 1996.
// Definitions:
// 1. A move instruction is said to be constrained if can by no means be
//    coalesced, that is, both of its destination and source are precolored, or
//    they interfere.
// 2. A node is said to be move-related if it appears in some non-constrained
//    move instruction that has not been coalesced or frozen.
//
// Propositions:
// 3. A node can be simplified (removed from the interference graph) if
//      a. it is non-move-related;
//      b. and it has fewer than K neighbors. In optimistic coloring, a node
//         with no less than K neighbors if there exists no node with fewer than
//         K neighbors, hoping that this will cause no problem.
// 4. For a non-constrained move instruction a -> b. Assume that a is not
//    precolored. If b is not precolored either, then it can be coalesced as
//    long as #{u | u is a neighbor of a or b, deg u >= K} < K. If b is
//    precolored, then a can be combined into b if for every neighbor t of a
//      a. t interfere with b;
//      b. or t is a machine register;
//      c. or deg(t) < K
//    (The reason why precolored nodes are handle separately is that we do not
//    record the adjacent list of precolored nodes.)

#ifndef MOCKER_REGISTER_ALLOCATION_H
#define MOCKER_REGISTER_ALLOCATION_H

#include <cassert>
#include <cstddef>
#include <functional>
#include <list>
#include <queue>
#include <stack>

#include "helper.h"
#include "liveness.h"
#include "nasm/helper.h"
#include "nasm/module.h"

namespace mocker {
namespace detail {
using Node = std::shared_ptr<nasm::Register>;
using MovInst = std::shared_ptr<nasm::Mov>;
using MovSet =
    std::unordered_set<MovInst, nasm::InstPtrHash, nasm::InstPtrEqual>;
using nasm::RegMap;
using nasm::RegSet;

inline bool isPrecolored(const Node &node) { return !nasm::isVReg(node); }

std::vector<Node> getNonPrecoloredNodes(LineIter funcBeg, LineIter funcEnd);

class InterferenceGraph {
public:
  void clearAndInit(const std::vector<Node> &nonPrecolored);

  void bindFunctions(
      std::function<void(const Node &)> enableMoves_,
      std::function<bool(const Node &)> isMoveRelated_,
      std::function<std::vector<MovInst>(const Node &)> freezeMoves_) {
    enableMoves = std::move(enableMoves_);
    isMoveRelated = std::move(isMoveRelated_);
    freezeMoves = std::move(freezeMoves_);
  }

  void build(LineIter funcBeg, LineIter funcEnd,
             const std::function<void(const MovInst &)> &pushWorklist,
             const std::function<void(const MovInst &)> &updateAssociatedMove);

  // Return whether the graph can be simplified
  bool simplify();

  bool freezeNode();

  bool spill();

public:
  bool isAdjacent(Node u, Node v) const;

  void decrementDegree(const Node &n);

  bool isOfHighDegree(const Node &n) const;

  // Initialize the node worklists by classifying the nodes.
  void initWorklists(const std::vector<Node> &initial);

  // Return the nodes to be spilled, that is, the nodes that can not been
  // colored, and the coloring
  std::pair<std::vector<Node>, RegMap<Node>> assignColors();

  std::vector<Node> getCurAdjList(const Node &node) const;

  Node getAlias(const Node &n) const;

  void classifyNode(const Node &n);

  bool conservative(const Node &x, const Node &y) const;

  bool isCoalesceAbleToAMachineReg(const Node &v, const Node &r);

  // merge v into u
  void combine(const Node &v, const Node &u);

private:
  void addEdge(Node u, Node v);

private:
  using NodePair = std::pair<Node, Node>;
  struct NodePairHash {
    std::size_t operator()(const NodePair &nodePair) const {
      return std::hash<std::string>{}(nodePair.first->getIdentifier() + "," +
                                      nodePair.second->getIdentifier());
    }
  };
  struct NodePairEqual {
    bool operator()(const NodePair &lhs, const NodePair &rhs) const {
      return lhs.first->getIdentifier() + "," + lhs.second->getIdentifier() ==
             rhs.first->getIdentifier() + "," + rhs.second->getIdentifier();
    }
  };

  std::function<void(const Node &)> enableMoves; // enable all associated moves
  std::function<bool(const Node &)> isMoveRelated;
  // freezeMoves should return the Moves being frozen, namely, the current
  // associated moves
  std::function<std::vector<MovInst>(const Node &)> freezeMoves;

  // Note that the interference graph will be modified during the process:
  // some nodes will be merged and some will be removed. Simplify simulating the
  // modification will lead to unaffordable cost. Hence, we use these different
  // data structures to record the current state.
  // edges: the edges in the current graph. This is used to answer whether two
  //     nodes are adjacent in the current graph. Redundant edges may exist
  //     since we do not remove the corresponding edges when a node is
  //     removed or merged.
  // originalAdjList: the adjacent list of the original graph. This will not be
  //    modified once the graph has been built. The adjacent lists of the
  //    precolored node are not recorded.
  // curDegree: the current degree of a node. For precolored nodes, their
  //    degree is set to be INF
  std::unordered_set<NodePair, NodePairHash, NodePairEqual> edges;
  RegMap<RegSet> originalAdjList;
  RegMap<std::size_t> curDegree;

  RegSet simplifiable; // low-degree non-move-related
  RegSet highDegree;   // high-degree
  RegSet freeze;       // low-degree move-related
  RegSet coalesced;    // having been coalesced
  RegSet removed;      // having been removed in the simplification phase

  RegMap<Node> alias;
  std::vector<Node> selectStack;
};

class MoveInfo {
public:
  void clearAndInit(const std::vector<Node> &nonPrecolored);

  bool isWorklistEmpty() const { return worklist.empty(); }

  void pushWorklist(const MovInst &mov) { worklist.emplace(mov); }

  void pushCoalescedMove(const MovInst &mov) { coalesced.emplace(mov); }

  void pushConstrainedMove(const MovInst &mov) { constrained.emplace(mov); }

  void pushActiveMove(const MovInst &mov) { active.emplace(mov); }

  MovInst popWorklist();

  void updateAssociatedMoves(const MovInst &mov);

  std::vector<MovInst> getCurAssociatedMoves(const Node &n);

  void mergeAssociatedMoves(const Node &u, const Node &v);

  std::vector<MovInst> freezeMoves(const Node &n);

  void enableMove(const MovInst &mov);

  bool isMoveRelated(const Node &n) const;

private:
  // For a move instruction M: u -> v,
  // if it is waiting for process, then it is in [worklist];
  // if we have already know that it can not be coalesced because u and v
  // interfere or the nodes are both precolored, then it is in [constrained];
  // if it can not be coalesced according to our conservative criterion, then
  // it is in [active] so that further simplification may enable it for
  // coalescing;
  // if we give up to coalesce M, then it is in [frozen];
  // if it has been coalesced, then it is in [coalesced]

  MovSet active, constrained, frozen, coalesced, worklist;

  RegMap<MovSet> associatedMoves;
};

} // namespace detail

namespace detail {

class RegisterAllocator {
public:
  RegisterAllocator(nasm::Section &section, LineIter funcBeg, LineIter funcEnd)
      : section(section), funcBeg(funcBeg), funcEnd(funcEnd) {}

  void allocate();

private:
  // Get a move instruction possible for coalescing and try to coalesce it
  bool coalesce();

  void color(const RegMap<Node> &coloring);

  void rewriteProgram(const std::vector<Node> &toBeSpilled);

private:
  nasm::Section &section;
  LineIter funcBeg, funcEnd;

  InterferenceGraph interG;
  MoveInfo moveInfo;
};

} // namespace detail
} // namespace mocker

namespace mocker {

nasm::Module allocateRegisters(const nasm::Module &module);
}

#endif // MOCKER_REGISTER_ALLOCATION_H
