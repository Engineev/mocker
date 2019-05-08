#include "function_inline.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <iostream>
#include <list>

#include "helper.h"
#include "ir/helper.h"
#include "optim/analysis/loop_info.h"

namespace mocker {

FunctionInline::FunctionInline(ir::Module &module) : ModulePass(module) {}

bool FunctionInline::operator()() {
  buildInlineable();
  std::cerr << "\nFunctionInline: ";
  for (auto &func : inlineable) {
    std::cerr << func << ", ";
  }
  std::cerr << std::endl;
  auto funcsCopy = module.getFuncs();

  for (auto &kv : funcsCopy) {
    auto &func = kv.second;
    if (func.isExternalFunc())
      continue;
    auto bbIter = func.getMutableBBs().begin();
    while (bbIter != func.getMutableBBs().end()) {
      auto callIter = std::find_if(
          bbIter->getMutableInsts().begin(), bbIter->getMutableInsts().end(),
          [this](const std::shared_ptr<ir::IRInst> &inst) {
            auto call = ir::dyc<ir::Call>(inst);
            if (!call)
              return false;
            return inlineable.find(call->getFuncName()) != inlineable.end();
          });

      if (callIter == bbIter->getMutableInsts().end()) {
        ++bbIter;
        continue;
      }
      auto call = ir::dyc<ir::Call>(*callIter);
      std::cerr << "inline " << call->getFuncName() << " into " << kv.first
                << std::endl;
      bbIter = inlineFunction(func, bbIter, callIter);
    }
  }

  module.getFuncs() = funcsCopy;
  for (auto &func : module.getFuncs())
    removeDeletedInsts(func.second);

  return false;
}

namespace {

std::size_t countInsts(const ir::FunctionModule &func, std::size_t threshold) {
  std::size_t preCount = 0;
  for (auto &bb : func.getBBs()) {
    preCount += bb.getInsts().size();
  }
  if (preCount > threshold)
    return threshold;

  LoopInfo loopTree;
  loopTree.init(func);

  // a^b
  auto power = [threshold](std::size_t a, std::size_t b) {
    std::size_t res = 1;
    for (std::size_t i = 0; i < b; ++i) {
      res *= a;
      res = std::min<std::size_t>(res, threshold);
    }
    return res;
  };

  std::size_t res = 0;
  for (auto &bb : func.getBBs()) {
    res += bb.getInsts().size() * power(10, loopTree.getDepth(bb.getLabelID()));
  }

  return res;
}

} // namespace

void FunctionInline::buildInlineable() {
  for (auto &kv : module.getFuncs()) {
    if (kv.second.isExternalFunc())
      continue;
    if (countInsts(kv.second, 201) <= 150) {
      inlineable.emplace(kv.first);
    }
  }
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

  // create a temporary variable to hold the return value
  std::shared_ptr<ir::Reg> retVal = nullptr;
  if (call->getDest()) {
    retVal = caller.makeTempLocalReg("retVal");
    caller.getFirstBB()->appendInstFront(std::make_shared<ir::Alloca>(retVal));
  }

  // move the subsequent instructions to a new BB
  auto succBBIter = caller.insertBBAfter(bbIter);
  succBBIter->getMutableInsts().splice(succBBIter->getMutableInsts().end(),
                                       bbIter->getMutableInsts(), callInstIter,
                                       bbIter->getMutableInsts().end());
  succBBIter->getMutableInsts().pop_front();
  if (call->getDest())
    succBBIter->getMutableInsts().emplace_front(
        std::make_shared<ir::Load>(call->getDest(), retVal));

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
      if (auto dest = ir::dycLocalReg(ir::getDest(inst))) {
        auto newDest = caller.makeTempLocalReg(dest->getIdentifier());
        inst = ir::copyWithReplacedDest(inst, newDest);
        newIdent[dest->getIdentifier()] = newDest->getIdentifier();
      }

      auto operands = ir::getOperandsUsed(inst);
      for (auto &operand : operands) {
        auto reg = ir::dycLocalReg(operand);
        if (!reg)
          continue;
        if (isParameter(callee, reg->getIdentifier())) {
          auto n = std::stol(reg->getIdentifier(), nullptr);
          operand = call->getArgs().at((std::size_t)n);
          continue;
        }
        operand = std::make_shared<ir::Reg>(newIdent.at(reg->getIdentifier()));
      }
      inst = ir::copyWithReplacedOperands(inst, operands);

      if (auto br = ir::dyc<ir::Branch>(inst)) {
        inst = std::make_shared<ir::Branch>(
            br->getCondition(),
            std::make_shared<ir::Label>(newLabelID.at(br->getThen()->getID())),
            std::make_shared<ir::Label>(newLabelID.at(br->getElse()->getID())));
      } else if (auto jump = ir::dyc<ir::Jump>(inst)) {
        inst = std::make_shared<ir::Jump>(std::make_shared<ir::Label>(
            newLabelID.at(jump->getLabel()->getID())));
      }

      if (auto alloca = ir::dyc<ir::Alloca>(inst)) {
        caller.getFirstBB()->getMutableInsts().push_front(std::move(inst));
        inst = std::make_shared<ir::Deleted>();
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
          retVal,
          ret->getVal() ? ret->getVal() : std::make_shared<ir::IntLiteral>(0)));
    }
    bb.appendInst(std::make_shared<ir::Jump>(
        std::make_shared<ir::Label>(succBBIter->getLabelID())));
  }

  return succBBIter;
}

} // namespace mocker