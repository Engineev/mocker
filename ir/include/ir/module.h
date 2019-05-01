#ifndef MOCKER_MODULE_H
#define MOCKER_MODULE_H

#include <list>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir_inst.h"

namespace mocker {
namespace ir {

using InstList = std::list<std::shared_ptr<IRInst>>;
using InstListIter = typename InstList::const_iterator;
template <class T> using LabelMap = std::unordered_map<std::size_t, T>;
using LabelSet = std::unordered_set<std::size_t>;

class FunctionModule;

class BasicBlock {
public:
  explicit BasicBlock(size_t labelID);
  BasicBlock(const BasicBlock &) = default;
  BasicBlock(BasicBlock &&) = default;
  BasicBlock &operator=(const BasicBlock &) = default;
  BasicBlock &operator=(BasicBlock &&) = default;

  std::size_t getLabelID() const { return labelID; }

  void setLabelID(std::size_t label) { labelID = label; }

  const InstList &getInsts() const { return insts; }

  InstList &getMutableInsts() { return insts; }

  void appendInst(std::shared_ptr<IRInst> inst);

  void appendInstFront(std::shared_ptr<IRInst> inst);

  void appendInstBeforeTerminator(std::shared_ptr<IRInst> inst);

  // check whether the last instruction is a terminator
  bool isCompleted() const;

  std::vector<std::size_t> getSuccessors() const;

private:
  std::size_t labelID;
  InstList insts;
};

using BasicBlockList = std::list<BasicBlock>;
using BBLIter = BasicBlockList::iterator;

class FunctionModule {
public:
  FunctionModule(std::string identifier, std::vector<std::string> args,
                 bool isExternal = false);
  FunctionModule(const FunctionModule &) = default;
  FunctionModule(FunctionModule &&) = default;
  FunctionModule &operator=(const FunctionModule &) = default;
  FunctionModule &operator=(FunctionModule &&) = default;

  BBLIter pushBackBB();

  BBLIter insertBBAfter(BBLIter iter);

  void sortBasicBlocks();

  const std::vector<std::string> &getArgs() const { return args; }

  const BasicBlockList &getBBs() const { return bbs; }

  BasicBlockList &getMutableBBs() { return bbs; }

  bool isExternalFunc() const { return isExternal; }

  const std::string &getIdentifier() const { return identifier; }

  std::shared_ptr<Reg> makeTempLocalReg(const std::string &hint = "");

public:
  void buildContext();

  const BasicBlock &getBasicBlock(std::size_t labelID) const;

  BasicBlock &getMutableBasicBlock(std::size_t labelID);

  BBLIter getFirstBB() { return bbs.begin(); }

  std::size_t getFirstBBLabel() const { return bbs.front().getLabelID(); }

  std::vector<std::size_t> getPredcessors(std::size_t bb) const;

private:
  std::string identifier;
  std::vector<std::string> args;
  BasicBlockList bbs;
  std::size_t bbsSz = 0;
  std::size_t tempRegCounter;
  bool isExternal = false;

private: // context
  std::unordered_map<std::size_t, std::reference_wrapper<BasicBlock>> bbMap;
  std::unordered_map<std::size_t, std::vector<std::size_t>> predecessors;
};

class GlobalVar {
public:
  GlobalVar(std::string label, std::string data);

  const std::string &getLabel() const { return label; }

  const std::string &getData() const { return data; }

private:
  std::string label, data;
};

class Module {
public:
  using GlobalVarList = std::list<GlobalVar>;
  using GlobalVarIter = GlobalVarList::iterator;
  using FuncsMap = std::unordered_map<std::string, FunctionModule>;

  const GlobalVarList &getGlobalVars() const { return globalVars; }

  GlobalVarList &getGlobalVars() { return globalVars; }

  const FuncsMap &getFuncs() const { return funcs; }

  FuncsMap &getFuncs() { return funcs; }

  FunctionModule &addFunc(std::string ident, FunctionModule func);

  FunctionModule &overwriteFunc(const std::string ident,
                                const FunctionModule &func);

  void addGlobalVar(std::string identifier, std::string data);

private:
  GlobalVarList globalVars;
  FuncsMap funcs;
};

} // namespace ir
} // namespace mocker

#endif // MOCKER_MODULE_H
