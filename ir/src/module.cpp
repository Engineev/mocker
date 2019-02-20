#include "module.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "defer.h"

namespace mocker {
namespace ir {

BasicBlock::BasicBlock(size_t labelID) : labelID(labelID) {}

void BasicBlock::appendInst(std::shared_ptr<IRInst> inst) {
  if (isCompleted())
    throw std::logic_error("Can't append insts into a completed block");
  insts.emplace_back(std::move(inst));
}

void BasicBlock::appendInstFront(std::shared_ptr<IRInst> inst) {
  insts.emplace_front(std::move(inst));
}

bool BasicBlock::isCompleted() const {
  if (insts.empty())
    return false;
  return (bool)std::dynamic_pointer_cast<Terminator>(insts.back());
}

std::vector<std::size_t> BasicBlock::getSuccessors() const {
  assert(isCompleted());
  const auto &lastInst = insts.back();
  if (std::dynamic_pointer_cast<Ret>(lastInst))
    return {};
  if (auto p = std::dynamic_pointer_cast<Jump>(lastInst))
    return {p->dest->id};
  if (auto p = std::dynamic_pointer_cast<Branch>(lastInst))
    return {p->then->id, p->else_->id};
  assert(false);
}

FunctionModule::FunctionModule(std::string identifier,
                               std::vector<std::string> args_, bool isExternal)
    : identifier(std::move(identifier)), args(std::move(args_)),
      isExternal(isExternal), tempRegCounter(args.size()) {}

BBLIter FunctionModule::pushBackBB() {
  bbs.emplace_back(bbsSz++);
  return --bbs.end();
}

BBLIter FunctionModule::insertBBAfter(BBLIter iter) {
  assert(iter != bbs.end());
  ++iter;
  return bbs.emplace(iter, bbsSz++);
}

void FunctionModule::buildContext() {
  bbMap.clear();

  bbMap.reserve(bbs.size());
  for (auto &bb : bbs)
    bbMap.emplace(bb.getLabelID(), bb);
  contextBuilt = true;
}

const BasicBlock &FunctionModule::getBasicBlock(std::size_t labelID) const {
  assert(contextBuilt);
  return bbMap.at(labelID);
}

BasicBlock &FunctionModule::getMutableBasicBlock(std::size_t labelID) {
  assert(contextBuilt);
  return bbMap.at(labelID);
}

std::shared_ptr<LocalReg>
FunctionModule::makeTempLocalReg(const std::string &hint) {
  return std::make_shared<LocalReg>(hint + std::to_string(tempRegCounter++));
}

FunctionModule &Module::addFunc(std::string ident, FunctionModule func) {
  auto p = funcs.emplace(std::move(ident), std::move(func));
  assert(p.second);
  return p.first->second;
}

void Module::appendGlobalVar(GlobalVarModule var) {
  globalVars.emplace_back(std::move(var));
}

} // namespace ir
} // namespace mocker
