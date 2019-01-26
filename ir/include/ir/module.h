#ifndef MOCKER_MODULE_H
#define MOCKER_MODULE_H

#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "ir_inst.h"

namespace mocker {
namespace ir {

using InstList = std::list<std::shared_ptr<IRInst>>;
using InstListIter = typename InstList::const_iterator;

class FunctionModule;

class BasicBlock {
public:
  BasicBlock(const std::reference_wrapper<FunctionModule> &func,
             size_t labelID);

  FunctionModule &getFuncRef() const { return func; }

  std::size_t getLabelID() const { return labelID; }

  const InstList &getInsts() const { return insts; }

  void appendFront(std::shared_ptr<IRInst> inst);

  void appendInst(std::shared_ptr<IRInst> inst);

  // check whether the last instruction is a terminator
  bool isCompleted() const;

private:
  std::reference_wrapper<FunctionModule> func;
  std::size_t labelID;
  InstList insts;
};

using BasicBlockList = std::list<BasicBlock>;
using BBLIter = BasicBlockList::iterator;

class FunctionModule {
public:
  FunctionModule(std::string identifier, std::vector<std::string> args,
                 bool isExternal = false);

  FunctionModule(FunctionModule &&) = default;
  FunctionModule &operator=(FunctionModule &&) = default;

  BBLIter pushBackBB();

  BBLIter insertBBAfter(BBLIter iter);

  const std::vector<std::string> &getArgs() const { return args; }

  const BasicBlockList &getBBs() const { return bbs; }

  bool isExternalFunc() const { return isExternal; }

  const std::string &getIdentifier() const { return identifier; }

private:
  std::string identifier;
  std::vector<std::string> args;
  BasicBlockList bbs;
  std::size_t bbsSz = 0;
  bool isExternal = false;
};

class GlobalVarModule {
public:
  explicit GlobalVarModule(std::string identifier)
      : identifier(std::move(identifier)) {}

  const std::string &getIdentifier() const { return identifier; }

  template <class InstType, class... Args> void emplaceInst(Args &&... args) {
    auto inst = std::make_shared<InstType>(std::forward<Args>(args)...);
    init.emplace_back(std::move(inst));
  }

  const InstList &getInit() const { return init; }

private:
  std::string identifier;
  InstList init;
};

struct Module {
  using GlobalVarList = std::list<GlobalVarModule>;
  using GlobalVarIter = GlobalVarList::iterator;

  std::list<GlobalVarModule> globalVars;
  std::unordered_map<std::string, FunctionModule> funcs;
};

} // namespace ir
} // namespace mocker

#endif // MOCKER_MODULE_H
