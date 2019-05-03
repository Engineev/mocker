#include "loopinv.h"

#include <cassert>
#include <queue>

#include "helper.h"
#include "set_operation.h"

#include "ir/printer.h"
#include <iostream>

namespace mocker {
namespace {

// For I: a = phi <b0, 0> <b1, 1> <b2, 2>, s = {0, 1} and preHeader = 3, the
// result is
//   I1: a = phi <b2, 2> <c, 3>
//   I2: c = phi <b0, 0> <b1, 1>
std::pair<std::shared_ptr<ir::Phi>, std::shared_ptr<ir::Phi>>
splitPhi(ir::FunctionModule &func, const std::shared_ptr<ir::Phi> &phi,
         const std::unordered_set<std::size_t> &s,
         const std::size_t preHeader) {
  std::vector<ir::Phi::Option> optionLeft, optionMoved;
  for (auto &option : phi->getOptions()) {
    if (isIn(s, option.second->getID()))
      optionMoved.emplace_back(option);
    else
      optionLeft.emplace_back(option);
  }
  if (optionMoved.empty()) {
    return {phi, nullptr};
  }

  auto preHeaderPhiDest = func.makeTempLocalReg();
  optionLeft.emplace_back(preHeaderPhiDest,
                          std::make_shared<ir::Label>(preHeader));
  return {
      std::make_shared<ir::Phi>(phi->getDest(), optionLeft),
      std::make_shared<ir::Phi>(preHeaderPhiDest, optionMoved),
  };
}

} // namespace

void LoopInvariantCodeMotion::insertPreHeaders() {
  const auto Preds = buildBlockPredecessors(func);

  for (auto &kv : loopTree.getLoops()) {
    auto header = kv.first;
    auto &loopNodes = kv.second;
    if (loopNodes.size() == 1) // is not a loop header
      continue;
    std::unordered_set<std::size_t> outerPreds(Preds.at(header).begin(),
                                               Preds.at(header).end());
    subSet(outerPreds, loopNodes);

    if (kv.first == func.getFirstBBLabel())
      continue;

    assert(!outerPreds.empty());

    // Insert a pre-header. Be careful with the phi-functions
    auto &headBB = func.getMutableBasicBlock(header);
    auto &preHeaderBB = *func.pushBackBB();
    auto preHeader = preHeaderBB.getLabelID();
    auto tmp = preHeaders.emplace(std::make_pair(header, preHeader)).second;
    assert(tmp);

    // adjust the terminators of the outer predecessors
    for (auto &pred : outerPreds) {
      auto &bb = func.getMutableBasicBlock(pred);
      auto &terminator = bb.getMutableInsts().back();
      terminator = replaceTerminatorLabel(terminator, header, preHeader);
    }

    // adjust the phi-functions
    for (auto &inst : headBB.getMutableInsts()) {
      const auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        break;
      auto newPhis = splitPhi(func, phi, outerPreds, preHeader);
      if (newPhis.second) {
        preHeaderBB.appendInst(newPhis.second);
        inst = newPhis.first;
      }
    }
    preHeaderBB.appendInst(
        std::make_shared<ir::Jump>(std::make_shared<ir::Label>(header)));
    simplifyPhiFunctions(headBB);
    simplifyPhiFunctions(preHeaderBB);
  }
  removeDeletedInsts(func);
}

bool LoopInvariantCodeMotion::operator()() {
  loopTree.init(func);
  insertPreHeaders();
  func.buildContext();
  LoopTree newLoopTree;
  newLoopTree.init(func);
  loopTree = newLoopTree;

  auto loopHeads = loopTree.postOrder();
  std::size_t cnt = 0;
  for (auto header : loopHeads) {
    if (header == func.getFirstBBLabel())
      continue;
    cnt += processLoop(header);
  }
  return cnt;
}

std::size_t LoopInvariantCodeMotion::processLoop(const std::size_t header) {
  const auto &loopNodes = loopTree.getLoops().at(header);
  if (loopNodes.size() == 1)
    return 0;

  UseDefChain useDef;
  useDef.init(func);
  auto invariant = findLoopInvariantComputation(header, useDef);
  hoist(loopNodes, invariant, preHeaders.at(header));
  return invariant.size();
}

std::unordered_set<ir::InstID>
LoopInvariantCodeMotion::findCandidateCalls(std::size_t header) {
  const auto &loopNodes = loopTree.getLoops().at(header);

  // First, we find all loop nodes that all paths from the header to it contain
  // no store.
  std::unordered_set<std::size_t> reachableNodes(loopNodes.begin(),
                                                 loopNodes.end());
  std::unordered_set<std::size_t> visited;
  std::function<void(std::size_t)> filterOut =
      [this, header, &filterOut, &reachableNodes, &visited](std::size_t cur) {
        if (isIn(visited, cur) || cur == header)
          return;
        visited.emplace(cur);
        for (auto &suc : func.getBasicBlock(cur).getSuccessors()) {
          if (isIn(reachableNodes, suc))
            reachableNodes.erase(reachableNodes.find(suc));
          filterOut(suc);
        }
      };
  for (auto &label : loopNodes) {
    auto &bb = func.getBasicBlock(label);
    for (auto &inst : bb.getInsts()) {
      if (ir::dyc<ir::Store>(inst)) {
        visited.clear();
        filterOut(bb.getLabelID());
        break;
      }
    }
  }

  // Then, for every reachable nodes, we collect all loads that no store exists
  // before it in the basic block
  std::unordered_set<ir::InstID> res;
  for (auto node : reachableNodes) {
    auto &bb = func.getBasicBlock(node);
    for (auto &inst : bb.getInsts()) {
      if (ir::dyc<ir::Load>(inst))
        res.emplace(inst->getID());
      if (ir::dyc<ir::Store>(inst))
        break;
    }
  }

  return res;
}

std::unordered_set<ir::InstID>
LoopInvariantCodeMotion::findLoopInvariantComputation(
    std::size_t header, const UseDefChain &useDef) {
  const auto &loopNodes = loopTree.getLoops().at(header);
  //  const auto candidateLoads = findCandidateCalls(header);

  std::unordered_set<ir::InstID> res;
  auto isLoopInvariant = [&res, &loopNodes, &useDef,
                          this](const std::shared_ptr<ir::IRInst> &inst) {
    auto operands = ir::getOperandsUsed(inst);
    for (auto &operand : operands) {
      auto reg = ir::dycLocalReg(operand);
      if (!reg) {
        continue;
      }
      if (isParameter(func, reg->getIdentifier()))
        continue;
      const auto &def = useDef.getDef(reg);
      if (isIn(res, def.getInst()->getID()))
        continue;
      if (isIn(loopNodes, def.getBBLabel())) {
        return false;
      }
    }
    return true;
  };

  std::queue<std::shared_ptr<ir::IRInst>> worklist;

  for (auto node : loopNodes) {
    const auto &bb = func.getBasicBlock(node);
    for (auto &inst : bb.getInsts()) {
      if (!ir::dyc<ir::Assign>(inst) && !ir::dyc<ir::ArithUnaryInst>(inst) &&
          !ir::dyc<ir::ArithBinaryInst>(inst) &&
          !ir::dyc<ir::RelationInst>(inst) && !ir::dyc<ir::Call>(inst))
        continue;
      if (auto call = ir::dyc<ir::Call>(inst)) {
        if (!funcAttr.isPure(call->getFuncName()))
          continue;
      }
      if (!isLoopInvariant(inst))
        continue;
      // if (ir::dyc<ir::Load>(inst)) {
      //  worklist.emplace(inst);
      // }
      if (auto call = ir::dyc<ir::Call>(inst)) {
        if (!funcAttr.isPure(call->getFuncName()))
          continue;
        worklist.emplace(inst);
      }
      res.emplace(inst->getID());
    }
  }

  std::unordered_set<ir::InstID> related;
  while (!worklist.empty()) {
    auto inst = worklist.front();
    worklist.pop();
    related.emplace(inst->getID());
    auto operands = ir::getOperandsUsed(inst);
    for (auto &operand : operands) {
      if (auto reg = ir::dycLocalReg(operand)) {
        auto def = useDef.getDef(reg).getInst();
        if (!isIn(related, def->getID()))
          worklist.emplace(def);
      }
    }
  }

  intersectSet(res, related);
  return res;
}

void LoopInvariantCodeMotion::hoist(
    const std::unordered_set<std::size_t> &loopNodes,
    const std::unordered_set<ir::InstID> &invariant, std::size_t preHeader) {
  auto &preHeaderBB = func.getMutableBasicBlock(preHeader);
  for (auto node : loopNodes) {
    auto &bb = func.getMutableBasicBlock(node);
    for (auto &inst : bb.getMutableInsts()) {
      if (!isIn(invariant, inst->getID()))
        continue;
      std::cerr << "... move " << ir::fmtInst(inst) << std::endl;
      preHeaderBB.appendInstBeforeTerminator(inst);
      inst = std::make_shared<ir::Deleted>();
    }
  }
  removeDeletedInsts(func);
}

} // namespace mocker
