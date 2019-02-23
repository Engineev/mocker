#include "builder_context.h"

#include <cassert>

namespace mocker {
namespace ir {

BuilderContext::BuilderContext(
    const BuilderContext::ASTIDMap<std::shared_ptr<mocker::ast::Type>>
        &exprType)
    : exprType(exprType) {}

std::shared_ptr<LocalReg>
BuilderContext::makeTempLocalReg(const std::string &hint) {
  return curFunc->makeTempLocalReg(hint);
}

std::shared_ptr<Addr> BuilderContext::getExprAddr(ast::NodeID id) const {
  return exprAddr.at(id);
}

void BuilderContext::setExprAddr(ast::NodeID id, std::shared_ptr<Addr> addr) {
  exprAddr[id] = std::move(addr);
}

Module &BuilderContext::getResult() { return module; }

void BuilderContext::appendInst(std::shared_ptr<IRInst> inst) {
  appendInst(curBasicBlock, std::move(inst));
}

void BuilderContext::appendInst(BBLIter bblIter, std::shared_ptr<IRInst> inst) {
  bbReside[inst->getID()] = bblIter;
  bblIter->appendInst(std::move(inst));
}

void BuilderContext::appendInstFront(BBLIter bblIter,
                                     std::shared_ptr<IRInst> inst) {
  bbReside[inst->getID()] = bblIter;
  bblIter->appendInstFront(std::move(inst));
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
  curFunc = &module.addFunc(std::move(ident), std::move(func));
  return getCurFunc();
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

const std::shared_ptr<ast::Type>
BuilderContext::getExprType(ast::NodeID id) const {
  return exprType.at(id);
}

void BuilderContext::initFuncCtx(std::size_t paramNum) {
  assert(loopEntry.empty());
  curBasicBlock = BBLIter();

  exprAddr.clear();
  bbReside.clear();
}

std::shared_ptr<GlobalReg>
BuilderContext::addStringLiteral(const std::string &literal) {
  // Note that what we store is a string object, i.e., length + content, instead
  // of the literal itself.

  auto iter = strLits.find(literal);
  if (iter != strLits.end())
    return iter->second;

  // !!! The member version must be hidden here
  std::size_t tempRegCounter = 0;
  auto makeTempLocalReg = [&tempRegCounter](std::string hint) {
    return std::make_shared<LocalReg>(hint + std::to_string(tempRegCounter++));
  };

  auto ident = "@_strlit_" + std::to_string(strLitCounter++);

  GlobalVarModule var(ident);

  // init
  auto strInstPtrPtr = std::make_shared<GlobalReg>(ident);
  var.emplaceInst<SAlloc>(strInstPtrPtr, 8);
  auto strInstPtr = makeTempLocalReg("strInstPtr");
  var.emplaceInst<Malloc>(strInstPtr, std::make_shared<IntLiteral>(16));
  var.emplaceInst<Store>(strInstPtrPtr, strInstPtr);

  auto litLen = std::make_shared<IntLiteral>(literal.length());
  var.emplaceInst<Store>(strInstPtr, litLen);
  auto contentPtr = makeTempLocalReg("contentPtr");
  var.emplaceInst<Malloc>(contentPtr, litLen);
  auto contentPtrPtr = makeTempLocalReg("contentPtrPtr");
  var.emplaceInst<ArithBinaryInst>(contentPtrPtr, ArithBinaryInst::Add,
                                   strInstPtr, std::make_shared<IntLiteral>(8));
  var.emplaceInst<Store>(contentPtrPtr, contentPtr);
  var.emplaceInst<StrCpy>(contentPtr, literal);

  module.appendGlobalVar(std::move(var));
  strLits.emplace(literal, strInstPtrPtr);
  return strInstPtrPtr;
}

void BuilderContext::addGlobalVar(GlobalVarModule var) {
  module.appendGlobalVar(std::move(var));
}

} // namespace ir
} // namespace mocker