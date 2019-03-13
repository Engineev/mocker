#ifndef MOCKER_SSA_H
#define MOCKER_SSA_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "opt_pass.h"

namespace mocker {

// Promote memory references to register references. An implementation of the
// standard SSA construction algorithm.
class SSAConstruction : public FuncPass {
public:
  explicit SSAConstruction(ir::FunctionModule &func);

  bool operator()() override;

private:
  template <class T>
  bool isIn(const T &key, const std::unordered_set<T> &s) const {
    return s.find(key) != s.end();
  }

private:
  template <class V> using LabelMap = std::unordered_map<std::size_t, V>;
  template <class V> using IRMap = std::unordered_map<ir::InstID, V>;
  using LabelSet = std::unordered_set<std::size_t>;

  struct BBInfo {
    LabelSet dominating, dominators;
    std::size_t immediateDominator = std::size_t(-1);
    std::vector<std::size_t> dominanceFrontier;
  };
  struct DTNode {
    explicit DTNode(ir::BasicBlock &content) : content(content) {}
    std::reference_wrapper<ir::BasicBlock> content;
    std::vector<std::shared_ptr<DTNode>> children;
  };
  struct Definition {
    Definition(size_t blockLabel, std::shared_ptr<const ir::Addr> val)
        : blockLabel(blockLabel), val(std::move(val)) {}

    std::size_t blockLabel;
    std::shared_ptr<const ir::Addr> val;
  };

  std::unordered_set<std::string> varNames;
  LabelMap<BBInfo> bbInfo;
  std::shared_ptr<DTNode> root; // the root of the dominator tree

  IRMap<std::string> varDefined;
  std::unordered_map<std::string, std::string> reachingDef;
  std::unordered_map<std::string, std::size_t> bbDefined;

private:
  void computeAuxiliaryInfo();

  // Prerequisites: computeAuxiliaryInfo
  void printAuxiliaryInfo();

  // Prerequisites: None
  void buildDominating();

  void buildDominatingImpl(std::size_t node);

  // whether [u] dominates [v] ?
  // Prerequisites: buildDominating
  bool isDominating(std::size_t u, std::size_t v) const;

  // whether [u] strictly dominates [v]
  // Prerequisites: buildDominating
  bool isStrictlyDominating(std::size_t u, std::size_t v) const;

  // Prerequisites: buildDominating
  void buildDominators();

  // Prerequisites: buildDominating, buildDominators
  // Block n1 is said to be the immediate dominator of block n2 if
  // (1) n1 strictly dominates n2
  // (2) n1 does not strictly dominates any block that strictly dominates n2
  void buildImmediateDominator();

  void buildImmediateDominatorImpl(std::size_t node);

  // Prerequisites: buildImmediateDominator
  void buildDominatorTree();

  // Prerequisites: buildImmediateDominator
  void buildDominatingFrontier();

private:
  void insertPhiFunctions();

  // insert the incomplete phi-functions and update [varDefined]
  void insertPhiFunctions(const std::string &varName);

  // Collection the definition points of a given variable, replace each
  // Store by the corresponding Assign and update [varDefined]
  std::vector<Definition> collectAndReplaceDefs(const std::string &name);

private:
  void renameVariables();

  void renameVariablesImpl(const std::shared_ptr<DTNode> &curNode);

  void updateReachingDef(const std::string &varName, std::size_t label);
};

} // namespace mocker

namespace mocker {

// Replace a phi-function by an Assign when possible
class SimplifyPhiFunctions : public FuncPass {
public:
  explicit SimplifyPhiFunctions(ir::FunctionModule &func);

  bool operator()() override;
};

} // namespace mocker

namespace mocker {

class SSADestruction : public FuncPass {
public:
  explicit SSADestruction(ir::FunctionModule &func);

  bool operator()() override;

private:
  void insertAllocas();

  // Split all critical edges by inserting dummy basic blocks.
  void splitCriticalEdges();

  struct Copy {
    Copy(std::shared_ptr<ir::LocalReg> dest, std::shared_ptr<ir::Addr> val)
        : dest(std::move(dest)), val(std::move(val)) {}

    std::shared_ptr<ir::LocalReg> dest;
    std::shared_ptr<ir::Addr> val;
  };

  void replacePhisWithParallelCopies();

  void sequentializeParallelCopies();

private:
  std::unordered_map<std::size_t, std::list<Copy>> parallelCopies;
  // For each phi-function a=phi(...), we create an AllocVar instruction for a,
  // and [address] is the mapping from the identifiers to the AllocVar's
  std::unordered_map<std::string, std::shared_ptr<ir::LocalReg>> addresses;
};

} // namespace mocker

#endif // MOCKER_SSA_H
