#include "function_inline.h"

#include <cassert>
#include <cctype>
#include <list>

#include "helper.h"
#include "ir/helper.h"

namespace mocker {

FunctionInline::FunctionInline(ir::Module &module) : ModulePass(module) {}

void FunctionInline::operator()() {
  auto funcsCopy = module.getFuncs();

  for (auto &kv : funcsCopy) {
    auto &func = kv.second;
    if (func.isExternalFunc())
      continue;
    auto bbIter = func.getMutableBBs().begin();
    while (bbIter != func.getMutableBBs().end()) {
      auto callIter = bbIter->getMutableInsts().begin();
      while (callIter != bbIter->getMutableInsts().end() &&
             !ir::dyc<ir::Call>(*callIter)) {
        ++callIter;
      }
      if (callIter == bbIter->getMutableInsts().end()) {
        ++bbIter;
        continue;
      }
      bbIter = inlineFunction(func, bbIter, callIter);
    }
  }

  module.getFuncs() = funcsCopy;
}

bool FunctionInline::isParameter(const ir::FunctionModule &func,
                                 const std::string &identifier) const {
  if (!std::isdigit(identifier[0]))
    return false;

  std::size_t ed = 0;
  auto n = std::stoll(identifier, &ed);
  if (ed != identifier.size())
    return false;
  return n < (int)func.getArgs().size();
}

ir::BBLIter FunctionInline::inlineFunction(ir::FunctionModule &caller,
                                           ir::BBLIter bbIter,
                                           ir::InstListIter callInstIter) {
  auto call = ir::dyc<ir::Call>(*callInstIter);
  // do not use a reference here
  auto callee = module.getFuncs().at(call->getFuncName());
  if (callee.isExternalFunc())
    return ++bbIter;

  //    std::cerr << ir::fmtInst(call) << std::endl;

  // create a temporary variable to hold the return value
  std::shared_ptr<ir::Addr> retVal = nullptr;
  if (call->getDest()) {
    retVal = caller.makeTempLocalReg("retVal");
    caller.getFirstBB()->appendInstFront(
        std::make_shared<ir::Alloca>(retVal, 8));
  }

  // move the subsequent instructions to a new BB
  auto succBBIter = caller.insertBBAfter(bbIter);
  succBBIter->getMutableInsts().splice(succBBIter->getMutableInsts().end(),
                                       bbIter->getMutableInsts(), callInstIter,
                                       bbIter->getMutableInsts().end());
  succBBIter->getMutableInsts().pop_front();
  if (call->getDest())
    succBBIter->getMutableInsts().emplace_front(
        std::make_shared<ir::Load>(ir::copy(call->getDest()), retVal));

  // insert new BBs
  std::unordered_map<std::size_t, std::size_t> newLabelID;
  for (auto riter = callee.getBBs().rbegin(); riter != callee.getBBs().rend();
       ++riter) {
    const auto &bb = *riter;
    auto &curBB = *caller.insertBBAfter(bbIter);
    curBB.getMutableInsts() = bb.getInsts();
    newLabelID[bb.getLabelID()] = curBB.getLabelID();
  }
  bbIter->appendInst(std::make_shared<ir::Jump>(std::make_shared<ir::Label>(
      newLabelID.at(callee.getFirstBB()->getLabelID()))));

  // rename identifiers & LabelID; move alloca's to the first block
  std::unordered_map<std::string, std::string> newIdent;
  auto insertedBeg = bbIter;
  ++insertedBeg;
  for (auto iter = insertedBeg; iter != succBBIter; ++iter) {
    auto &bb = *iter;
    for (auto &inst : bb.getMutableInsts()) {
      assert(!ir::dyc<ir::Phi>(inst));
      if (auto dest = ir::cdyc<ir::LocalReg>(ir::getDest(inst))) {
        auto newDest = caller.makeTempLocalReg(dest->identifier);
        inst = ir::copyWithReplacedDest(inst, newDest);
        newIdent[dest->identifier] = newDest->identifier;
      }

      auto operands = ir::getOperandsUsed(inst);
      for (auto &operand : operands) {
        auto reg = ir::cdyc<ir::LocalReg>(operand);
        if (!reg)
          continue;
        if (isParameter(callee, reg->identifier)) {
          auto n = std::stol(reg->identifier, nullptr);
          operand = call->getArgs().at((std::size_t)n);
          continue;
        }
        operand = std::make_shared<ir::LocalReg>(newIdent.at(reg->identifier));
      }
      inst = ir::copyWithReplacedOperands(inst, operands);

      if (auto br = ir::dyc<ir::Branch>(inst)) {
        inst = std::make_shared<ir::Branch>(
            ir::copy(br->getCondition()),
            std::make_shared<ir::Label>(newLabelID.at(br->getThen()->id)),
            std::make_shared<ir::Label>(newLabelID.at(br->getElse()->id)));
      } else if (auto jump = ir::dyc<ir::Jump>(inst)) {
        inst = std::make_shared<ir::Jump>(
            std::make_shared<ir::Label>(newLabelID.at(jump->getLabel()->id)));
      }

      if (auto alloca = ir::dyc<ir::Alloca>(inst)) {
        caller.getFirstBB()->getMutableInsts().push_front(std::move(inst));
        inst = std::make_shared<ir::Comment>("deleted");
      }
    }
  }

  // rewrite Ret's
  for (auto iter = insertedBeg; iter != succBBIter; ++iter) {
    auto &bb = *iter;
    auto ret = ir::dyc<ir::Ret>(bb.getMutableInsts().back());
    if (!ret)
      continue;
    bb.getMutableInsts().pop_back();
    if (call->getDest()) {
      bb.appendInst(std::make_shared<ir::Store>(
          retVal, ret->getVal() ? ir::copy(ret->getVal())
                                : std::make_shared<ir::IntLiteral>(0)));
    }
    bb.appendInst(std::make_shared<ir::Jump>(
        std::make_shared<ir::Label>(succBBIter->getLabelID())));
  }

  return succBBIter;
}

} // namespace mocker