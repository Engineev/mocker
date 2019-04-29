#ifndef MOCKER_SSA_H
#define MOCKER_SSA_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "analysis/dominance.h"
#include "opt_pass.h"

namespace mocker {

// Promote memory references to register references. An implementation of the
// standard SSA construction algorithm.
class SSAConstruction : public FuncPass {
public:
  explicit SSAConstruction(ir::FunctionModule &func);

  bool operator()() override;

private:
  struct Definition {
    Definition(size_t blockLabel, std::shared_ptr<ir::Addr> val)
        : blockLabel(blockLabel), val(std::move(val)) {}

    std::size_t blockLabel;
    std::shared_ptr<ir::Addr> val;
  };
  using LabelSet = std::unordered_set<std::size_t>;
  template <class V> using LabelMap = std::unordered_map<std::size_t, V>;
  template <class V> using IRMap = std::unordered_map<ir::InstID, V>;

private:
  void insertPhiFunctions();

  // insert the incomplete phi-functions and update [varDefined]
  void insertPhiFunctions(const std::string &varName);

  // Collection the definition points of a given variable, replace each
  // Store by the corresponding Assign and update [varDefined]
  std::vector<Definition> collectAndReplaceDefs(const std::string &name);

private:
  void renameVariables();

  void renameVariablesImpl(std::size_t curRoot);

  void updateReachingDef(const std::string &varName, std::size_t label);

private:
  std::unordered_set<std::string> varNames;
  DominatorTree dominatorTree;
  IRMap<std::string> varDefined;
  std::unordered_map<std::string, std::string> reachingDef;
  std::unordered_map<std::string, std::size_t> bbDefined;
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
    Copy(std::shared_ptr<ir::Reg> dest, std::shared_ptr<ir::Addr> val)
        : dest(std::move(dest)), val(std::move(val)) {}

    std::shared_ptr<ir::Reg> dest;
    std::shared_ptr<ir::Addr> val;
  };

  void replacePhisWithParallelCopies();

  void sequentializeParallelCopies();

private:
  std::unordered_map<std::size_t, std::list<Copy>> parallelCopies;
  // For each phi-function a=phi(...), we create an Alloca instruction for a,
  // and [address] is the mapping from the identifiers to the Alloca's
  std::unordered_map<std::string, std::shared_ptr<ir::Reg>> addresses;
};

} // namespace mocker

#endif // MOCKER_SSA_H
