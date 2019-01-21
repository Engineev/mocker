#include "builder_context.h"

#include <cassert>

namespace mocker {
namespace ir {

std::shared_ptr<LocalReg>
BuilderContext::makeTempLocalReg(const std::string &hint) {
  return std::make_shared<LocalReg>(hint + std::to_string(tempRegCounter++));
}

std::shared_ptr<Addr> BuilderContext::getExprAddr(ast::NodeID id) const {
  return exprAddr.at(id);
}

void BuilderContext::setExprAddr(ast::NodeID id, std::shared_ptr<Addr> addr) {
  exprAddr[id] = std::move(addr);
}

const Module &BuilderContext::getResult() { return module; }

void BuilderContext::appendInst(std::shared_ptr<IRInst> inst) {
  appendInst(curBasicBlock, std::move(inst));
}

void BuilderContext::appendInst(BBLIter bblIter, std::shared_ptr<IRInst> inst) {
  bbReside[inst->getID()] = bblIter;
  bblIter->appendInst(std::move(inst));
}

void BuilderContext::appendInst(const std::string &funcName,
                                std::shared_ptr<IRInst> inst) {
  auto &func = module.funcs.at(funcName);
  func.insertAtBeginning(std::move(inst));
}

void BuilderContext::setCurBasicBlock(BBLIter val) { curBasicBlock = val; }

BBLIter BuilderContext::getCurBasicBlock() const { return curBasicBlock; }

const std::shared_ptr<Label> &BuilderContext::getCurLoopEntry() const {
  assert(!loopEntry.empty());
  return loopEntry.top();
}

void BuilderContext::pushLoopEntry(std::shared_ptr<Label> entry) {
  loopEntry.emplace(std::move(entry));
}

void BuilderContext::popLoopEntry() { loopEntry.pop(); }

const std::shared_ptr<Label> &BuilderContext::getCurLoopSuccessor() const {
  assert(!loopSuccessor.empty());
  return loopSuccessor.top();
}

void BuilderContext::pushLoopSuccessor(std::shared_ptr<Label> successor) {
  loopSuccessor.emplace(std::move(successor));
}

void BuilderContext::popLoopSuccessor() { loopSuccessor.pop(); }

FunctionModule &BuilderContext::addFunc(FunctionModule func) {
  auto ident = func.getIdentifier();
  auto p = module.funcs.emplace(ident, std::move(func));
  assert(p.second);
  return p.first->second;
}

void BuilderContext::addClassLayout(std::string className, ClassLayout layout) {
  classLayout.emplace(std::move(className), std::move(layout));
}

std::size_t BuilderContext::getClassSize(const std::string &className) const {
  return classLayout.at(className).size;
}

std::size_t BuilderContext::getOffset(const std::string &className,
                                      const std::string &varName) const {
  return classLayout.at(className).offset.at(varName);
}

void BuilderContext::setExprType(
    std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>> exprType_) {
  exprType = std::move(exprType_);
}

const std::shared_ptr<ast::Type>
BuilderContext::getExprType(ast::NodeID id) const {
  return exprType.at(id);
}

void BuilderContext::initFuncCtx() {
  assert(loopEntry.empty());
  tempRegCounter = 0;
  curBasicBlock = BBLIter();

  exprAddr.clear();
  bbReside.clear();
}

std::shared_ptr<GlobalReg>
BuilderContext::addStringLiteral(const std::string &literal) {
  auto iter = strLits.find(literal);
  if (iter != strLits.end())
    return iter->second;

  auto ident = "@_strlit_" + std::to_string(strLitCounter);
  module.globalVars.emplace_back(ident, literal.size(), literal);
  auto reg = std::make_shared<GlobalReg>(std::move(ident));
  strLits.emplace(literal, reg);
  return reg;
}

void BuilderContext::addGlobalVar(std::string identifier, size_t size,
                                  std::string data) {
  module.globalVars.emplace_back(std::move(identifier), size, std::move(data));
}

} // namespace ir
} // namespace mocker