#include "builder_context.h"

#include <cassert>
#include <cstring>

#include "ir/helper.h"

namespace mocker {
namespace ir {

std::shared_ptr<Reg> BuilderContext::makeTempLocalReg(const std::string &hint) {
  return curFunc->makeTempLocalReg(hint);
}

std::shared_ptr<Addr> BuilderContext::getExprAddr(ast::NodeID id) const {
  return exprAddr.at(id);
}

void BuilderContext::setExprAddr(ast::NodeID id, std::shared_ptr<Addr> addr) {
  exprAddr[id] = std::move(addr);
}

Module &BuilderContext::getResult() {
  // call _init_global_vars
  auto &insts = module.getFuncs().at("main").getFirstBB()->getMutableInsts();
  auto iter = insts.begin();
  while (ir::dyc<ir::Alloca>(*iter))
    ++iter;
  insts.emplace(iter,
                std::make_shared<ir::Call>(std::string("_init_global_vars")));

  emplaceGlobalInitInst<ir::Ret>();
  return module;
}

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

std::shared_ptr<Reg>
BuilderContext::addStringLiteral(const std::string &literal) {
  // Note that what we store is a string object, i.e., length + content, instead
  // of the literal itself.

  auto iter = strLits.find(literal);
  if (iter != strLits.end())
    return iter->second;

  auto ident = "@_strlit_" + std::to_string(strLitCounter++);
  addGlobalVar(ident);
  auto instPtr = std::make_shared<Reg>(ident);

  auto strInstIdent = ident + "c";
  char buffer[8 + literal.size() + 1];
  *(std::int64_t *)buffer = literal.size();
  std::strcpy(buffer + 8, literal.c_str());
  std::string data(buffer, buffer + 8 + literal.size());
  addGlobalVar(strInstIdent, data);

  auto &func = module.getFuncs().at("_init_global_vars");
  auto &bb = func.getMutableBBs().back();
  assert(!bb.isCompleted());
  auto contentPtr = func.makeTempLocalReg();
  bb.appendInst(std::make_shared<ArithBinaryInst>(
      contentPtr, ArithBinaryInst::Add, std::make_shared<Reg>(strInstIdent),
      std::make_shared<IntLiteral>(8)));
  bb.appendInst(std::make_shared<Store>(instPtr, contentPtr));

  strLits.emplace(literal, instPtr);
  return instPtr;
}

void BuilderContext::addGlobalVar(std::string ident, std::string data) {
  module.addGlobalVar(std::move(ident), std::move(data));
}

} // namespace ir
} // namespace mocker