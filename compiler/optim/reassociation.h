// Reassociate additions an subtractions

#ifndef MOCKER_REASSOCIATION_H
#define MOCKER_REASSOCIATION_H

#include <cassert>
#include <queue>

#include "analysis/defuse.h"
#include "opt_pass.h"
#include "set_operation.h"

namespace mocker {
namespace detail {
class ReassociationImpl {
public:
  ReassociationImpl(ir::BasicBlock &bb, ir::FunctionModule &func,
                    const DefUseChain &defUse, const UseDefChain &useDef)
      : bb(bb), func(func), defUse(defUse), useDef(useDef) {}

  bool operator()();

private:
  struct Node {
    enum Type {
      Root,
      Literal,
      Leaf,

      Add,
      Sub,
      Neg,
      Assign,
    };

    Node(Type type, std::shared_ptr<ir::Addr> value)
        : type(type), value(std::move(value)) {}

    Type type;
    std::shared_ptr<ir::Addr> value;
    std::vector<std::shared_ptr<Node>> children;
  };

  void findRoots();

  std::shared_ptr<Node> buildTrees(const std::shared_ptr<ir::Addr> &node,
                                   bool firstNode);

  std::vector<std::pair<bool /* positive */, std::shared_ptr<ir::Addr>>>
  flatten(const std::shared_ptr<Node> &root);

  bool areSameAddrs(const std::shared_ptr<ir::Addr> &lhs,
                    const std::shared_ptr<ir::Addr> &rhs);

  std::vector<std::pair<bool, std::shared_ptr<ir::Addr>>>
  cancel(const std::vector<std::pair<bool, std::shared_ptr<ir::Addr>>> &nodes);

  struct RankedNode {
    RankedNode(bool positive, std::shared_ptr<ir::Addr> value, int rank)
        : positive(positive), value(std::move(value)), rank(rank) {}

    bool operator<(const RankedNode &rhs) const { return rank < rhs.rank; }

    bool positive;
    std::shared_ptr<ir::Addr> value;
    int rank;
  };

  void rankNodes(const std::shared_ptr<ir::Reg> &root);

  void rebuild(const std::shared_ptr<ir::Reg> &root);

  void rebuild(std::priority_queue<RankedNode> &q,
               std::list<std::shared_ptr<ir::IRInst>>::iterator pos);

private:
  ir::BasicBlock &bb;
  ir::FunctionModule &func;
  const DefUseChain &defUse;
  const UseDefChain &useDef;

  ir::RegSet roots;

  // the map mapping each root register to its tree
  ir::RegMap<std::shared_ptr<Node>> trees;
  ir::RegMap<std::vector<std::pair<bool, std::shared_ptr<ir::Addr>>>>
      flattenedNodes;
  ir::RegMap<int> rootRank;
  ir::RegMap<std::priority_queue<RankedNode>> rankedNodes;
  ir::RegSet rebuilt;

  ir::RegSet doNotRebuild;
};
} // namespace detail
} // namespace mocker

namespace mocker {

class Reassociation : public FuncPass {
public:
  explicit Reassociation(ir::FunctionModule &func) : FuncPass(func) {}

  bool operator()() override {
    DefUseChain defUse;
    UseDefChain useDef;
    defUse.init(func);
    useDef.init(func);

    for (auto &bb : func.getMutableBBs()) {
      detail::ReassociationImpl(bb, func, defUse, useDef)();
    }

    return false;
  }
};

} // namespace mocker

#endif // MOCKER_REASSOCIATION_H
