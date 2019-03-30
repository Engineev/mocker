#include "ssa.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <queue>

#include "helper.h"
#include "ir/helper.h"

#include "ir/printer.h"

namespace mocker {

SSAConstruction::SSAConstruction(ir::FunctionModule &func) : FuncPass(func) {}

bool SSAConstruction::operator()() {
  computeAuxiliaryInfo();
  insertPhiFunctions();
  renameVariables();
  return false;
}

void SSAConstruction::computeAuxiliaryInfo() {
  buildDominating();
  buildDominators();
  buildImmediateDominator();
  buildDominatorTree();
  buildDominatingFrontier();

  //  printAuxiliaryInfo();
}

void SSAConstruction::printAuxiliaryInfo() {
  std::cerr << "ImmediateDominator:\n";
  for (auto &kv : bbInfo) {
    if (kv.first == func.getFirstBB()->getLabelID())
      continue;
    std::cerr << "idom(" << kv.first << ") = " << kv.second.immediateDominator
              << std::endl;
  }

  std::cerr << "children in the dominator tree:\n";
  std::function<void(const std::shared_ptr<DTNode> &)> printDTChildren =
      [&](const std::shared_ptr<DTNode> &cur) {
        std::cerr << cur->content.get().getLabelID() << ": ";
        for (auto &child : cur->children)
          std::cerr << child->content.get().getLabelID() << ", ";
        std::cerr << std::endl;
        for (auto &child : cur->children)
          printDTChildren(child);
      };
  printDTChildren(root);
}

void SSAConstruction::buildDominating() {
  for (const auto &bb : func.getBBs())
    buildDominatingImpl(bb.getLabelID());
}

void SSAConstruction::buildDominatingImpl(std::size_t node) {
  // Construct the set [avoidable] of nodes which are reachable from the entry
  // without passing [node]. The nodes dominated by [node] are just the nodes
  // which are not in [avoidable].
  LabelSet avoidable;
  std::function<void(std::size_t cur)> visit = [&visit, &avoidable, node,
                                                this](std::size_t cur) {
    if (cur == node || avoidable.find(cur) != avoidable.end())
      return;
    avoidable.emplace(cur);
    for (auto suc : func.getBasicBlock(cur).getSuccessors())
      visit(suc);
  };
  visit(func.getFirstBB()->getLabelID());

  auto &dominating = bbInfo[node].dominating;
  for (const auto &bb : func.getBBs()) {
    if (!isIn(bb.getLabelID(), avoidable))
      dominating.emplace(bb.getLabelID());
  }
}

bool SSAConstruction::isDominating(std::size_t u, std::size_t v) const {
  const auto &d = bbInfo.at(u).dominating;
  return d.find(v) != d.end();
}

bool SSAConstruction::isStrictlyDominating(std::size_t u, std::size_t v) const {
  return u != v && isDominating(u, v);
}

void SSAConstruction::buildDominators() {
  for (const auto &infoPair : bbInfo) {
    auto u = infoPair.first;
    const auto &info = infoPair.second;
    for (auto dominatedByU : info.dominating)
      bbInfo[dominatedByU].dominators.emplace(u);
  }
}

void SSAConstruction::buildImmediateDominator() {
  // For a faster algorithm, see [Lengauer, Tarjan, 1979]

  for (const auto &bb : func.getBBs())
    if (bb.getLabelID() != func.getFirstBB()->getLabelID())
      buildImmediateDominatorImpl(bb.getLabelID());
}

void SSAConstruction::buildImmediateDominatorImpl(std::size_t node) {
  auto &info = bbInfo.at(node);

  for (auto dominator : info.dominators) {
    if (dominator == node) // is not strict
      continue;
    bool flag = true;
    for (auto otherDominator : info.dominators) {
      if (otherDominator == node)
        continue;
      if (isStrictlyDominating(dominator, otherDominator)) {
        flag = false;
        break;
      }
    }
    if (flag) {
      info.immediateDominator = dominator;
      return;
    }
  }
  assert(false); // The immediate dominator of a node must exist.
}

void SSAConstruction::buildDominatorTree() {
  LabelMap<std::shared_ptr<DTNode>> nodePool;
  for (auto &bb : func.getMutableBBs())
    nodePool.emplace(bb.getLabelID(), std::make_shared<DTNode>(bb));
  root = nodePool.at(func.getFirstBB()->getLabelID());
  for (auto &infoPair : bbInfo) {
    auto child = infoPair.first;
    if (child == func.getFirstBB()->getLabelID())
      continue;
    auto pnt = infoPair.second.immediateDominator;
    nodePool.at(pnt)->children.emplace_back(nodePool.at(child));
  }

  // check
  //    LabelSet visited;
  //    std::function<void(const std::shared_ptr<DTNode> &)> visit =
  //        [&visit, &visited](const std::shared_ptr<DTNode> & cur) {
  //      auto label = cur->content.get().getLabelID();
  //      if (visited.find(label) != visited.end())
  //        std::terminate();
  //      visited.emplace(label);
  //      for (const auto & child : cur->children)
  //        visit(child);
  //    };
  //    visit(root);
}

void SSAConstruction::buildDominatingFrontier() {
  auto collectCFGEdge = [this] {
    std::vector<std::pair<std::size_t, std::size_t>> res;
    for (const auto &bb : func.getBBs()) {
      auto succ = bb.getSuccessors();
      for (auto to : succ)
        res.emplace_back(bb.getLabelID(), to);
    }
    return res;
  };

  auto edges = collectCFGEdge();
  for (const auto &edge : edges) {
    auto a = edge.first, b = edge.second;
    auto x = a;
    while (!isStrictlyDominating(x, b)) {
      auto &info = bbInfo.at(x);
      info.dominanceFrontier.emplace_back(b);
      x = info.immediateDominator;
    }
  }
}

void SSAConstruction::insertPhiFunctions() {
  const auto &firstBB = *func.getFirstBB();
  for (const auto &inst : firstBB.getInsts()) {
    if (auto p = ir::dyc<ir::AllocVar>(inst)) {
      auto reg = ir::dycLocalReg(p->getDest());
      assert(reg);
      varNames.emplace(reg->getIdentifier());
    }
  }

  for (const auto &v : varNames)
    insertPhiFunctions(v);
}

void SSAConstruction::insertPhiFunctions(const std::string &varName) {
  LabelSet originalDefs, added;
  std::queue<Definition> remaining;
  auto defs = collectAndReplaceDefs(varName);
  for (const auto &def : defs) {
    originalDefs.emplace(def.blockLabel);
    remaining.push(def);
  }

  while (!remaining.empty()) {
    auto def = remaining.front();
    remaining.pop();
    const auto &frontier = bbInfo.at(def.blockLabel).dominanceFrontier;
    for (const auto &frontierBB : frontier) {
      if (isIn(frontierBB, added))
        continue;

      auto dest = func.makeTempLocalReg(varName);
      auto phi =
          std::make_shared<ir::Phi>(dest, std::vector<ir::Phi::Option>());
      varDefined[phi->getID()] = varName;
      bbDefined[dest->getIdentifier()] = frontierBB;
      auto &bbInst = func.getMutableBasicBlock(frontierBB);
      bbInst.appendInstFront(phi);

      added.emplace(frontierBB);
      if (!isIn(frontierBB, originalDefs))
        remaining.emplace(frontierBB, dest);
    }
  }
}

std::vector<SSAConstruction::Definition>
SSAConstruction::collectAndReplaceDefs(const std::string &name) {
  std::vector<Definition> res;
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      if (inst->getInstType() != ir::IRInst::Store)
        continue;

      auto p = std::static_pointer_cast<ir::Store>(inst);
      auto reg = ir::dycLocalReg(p->getAddr());
      if (!reg)
        continue;
      if (reg->getIdentifier() != name)
        continue;
      auto dest = func.makeTempLocalReg(name);
      res.emplace_back(bb.getLabelID(), reg);
      auto assign = std::make_shared<ir::Assign>(dest, p->getVal());
      inst = assign;
      varDefined[assign->getID()] = reg->getIdentifier();
      bbDefined[dest->getIdentifier()] = bb.getLabelID();
    }
  }
  return res;
}

void SSAConstruction::renameVariables() {
  for (const auto &name : varNames)
    reachingDef[name] = ".phi_nan";
  bbDefined[".phi_nan"] = func.getFirstBB()->getLabelID();

  renameVariablesImpl(root);
}

void SSAConstruction::renameVariablesImpl(
    const std::shared_ptr<SSAConstruction::DTNode> &curNode) {
  auto &bb = curNode->content.get();

  for (auto &inst : bb.getMutableInsts()) {
    if (auto p = ir::dyc<ir::Phi>(inst)) {
      auto iter = varDefined.find(p->getID());
      if (iter == varDefined.end())
        continue;
      auto varName = iter->second;
      updateReachingDef(varName, bb.getLabelID());

      auto newVarName = ir::dycLocalReg(p->getDest())->getIdentifier();
      reachingDef[newVarName] = reachingDef[varName];
      reachingDef[varName] = newVarName;
      continue;
    }
    if (auto p = ir::dyc<ir::Load>(inst)) {
      auto var = ir::dycLocalReg(p->getAddr());
      if (!var) // is a global variable
        continue;
      if (!isIn(var->getIdentifier(), varNames))
        continue;
      // I think that the corresponding line in the SSA book is wrong.
      updateReachingDef(var->getIdentifier(), bb.getLabelID());
      inst = std::make_shared<ir::Assign>(
          p->getDest(),
          std::make_shared<ir::Reg>(reachingDef.at(var->getIdentifier())));
      continue;
    }
    if (auto p = ir::dyc<ir::Assign>(inst)) {
      auto iter = varDefined.find(p->getID());
      if (iter == varDefined.end())
        continue;
      auto varName = iter->second;
      updateReachingDef(varName, bb.getLabelID());

      auto newVarName = ir::dycLocalReg(p->getDest())->getIdentifier();
      reachingDef[newVarName] = reachingDef[varName];
      reachingDef[varName] = newVarName;
    }
  }

  // Update the option lists of the phi-functions in the successors
  for (const auto &sucLabel : bb.getSuccessors()) {
    auto &sucBB = func.getMutableBasicBlock(sucLabel);
    for (auto &inst : sucBB.getMutableInsts()) {
      auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        break;

      std::vector<ir::Phi::Option> options;
      for (auto &oldOption : phi->getOptions())
        options.emplace_back(std::make_pair(oldOption.first, oldOption.second));

      auto iter = varDefined.find(phi->getID());
      if (iter == varDefined.end())
        continue;
      auto varName = iter->second;
      updateReachingDef(varName, bb.getLabelID());
      auto reachingDefOfV = reachingDef.at(varName);
      options.emplace_back(std::make_shared<ir::Reg>(reachingDefOfV),
                           std::make_shared<ir::Label>(bb.getLabelID()));
      inst = std::make_shared<ir::Phi>(phi->getDest(), std::move(options));
      varDefined[inst->getID()] = varName;
    }
  }

  for (const auto &child : curNode->children)
    renameVariablesImpl(child);
}

void SSAConstruction::updateReachingDef(const std::string &varName,
                                        std::size_t label) {
  if (reachingDef.find(varName) == reachingDef.end())
    return;

  auto r = reachingDef.at(varName);
  while (!isDominating(bbDefined.at(r), label))
    r = reachingDef.at(r);
  reachingDef.at(varName) = r;
}

} // namespace mocker

namespace mocker {

SimplifyPhiFunctions::SimplifyPhiFunctions(ir::FunctionModule &func)
    : FuncPass(func) {}

bool SimplifyPhiFunctions::operator()() {
  std::size_t cnt = 0;
  for (auto &bb : func.getMutableBBs()) {
    // find the insertion point
    auto insertionPoint = bb.getMutableInsts().begin();
    while (ir::dyc<ir::Phi>(*insertionPoint))
      ++insertionPoint;

    for (auto &inst : bb.getMutableInsts()) {
      auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        break;

      if (phi->getOptions().size() == 1) {
        ++cnt;
        auto assign = std::make_shared<ir::Assign>(phi->getDest(),
                                                   phi->getOptions()[0].first);
        inst = std::make_shared<ir::Deleted>();
        bb.getMutableInsts().emplace(insertionPoint, std::move(assign));
      }
    }
  }
  removeDeletedInsts(func);
  return cnt != 0;
}
} // namespace mocker

namespace mocker {

SSADestruction::SSADestruction(ir::FunctionModule &func) : FuncPass(func) {}

bool SSADestruction::operator()() {
  insertAllocas();
  splitCriticalEdges();
  //  ir::printFunc(func, std::cerr);
  replacePhisWithParallelCopies();
  sequentializeParallelCopies();
  return false;
}

void SSADestruction::insertAllocas() {
  std::vector<std::string> varNames;

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        break;
      varNames.emplace_back(phi->getDest()->getIdentifier());
    }
  }

  for (auto &varName : varNames) {
    auto addr = func.makeTempLocalReg();
    addresses[varName] = addr;
    func.getFirstBB()->appendInstFront(std::make_shared<ir::AllocVar>(addr));
  }
}

void SSADestruction::splitCriticalEdges() {
  auto preds = buildBlockPredecessors(func);
  // We will insert new blocks during the iteration. Hence, record [ed] first
  // and only append new blocks after [ed].
  for (auto iter = func.getMutableBBs().begin(),
            ed = func.getMutableBBs().end();
       iter != ed; ++iter) {
    auto &bb = *iter;
    const auto &predsOfBB = preds[bb.getLabelID()];
    if (predsOfBB.size() <= 1) // is not a critical edge
      continue;

    for (auto predLabel : predsOfBB) {
      auto &pred = func.getMutableBasicBlock(predLabel);
      auto succOfPred = pred.getSuccessors();
      if (succOfPred.size() == 1) // is not a critical edge
        continue;

      // split this critical edge
      auto &newBB = *func.pushBackBB();
      newBB.appendInst(std::make_shared<ir::Jump>(
          std::make_shared<ir::Label>(bb.getLabelID())));
      // rewrite the phi-functions in [bb]
      for (auto &inst : bb.getMutableInsts()) {
        auto phi = ir::dyc<ir::Phi>(inst);
        if (!phi)
          break;
        inst = replacePhiOption(phi, bb.getLabelID(), newBB.getLabelID());
      }

      pred.getMutableInsts().back() = replaceTerminatorLabel(
          pred.getInsts().back(), bb.getLabelID(), newBB.getLabelID());
    }
  }
}

void SSADestruction::replacePhisWithParallelCopies() {
  auto preds = buildBlockPredecessors(func);

  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        break;

      auto dest = ir::dycLocalReg(phi->getDest());
      for (auto &option : phi->getOptions())
        parallelCopies[option.second->getID()].emplace_back(
            ir::dycLocalReg(dest), option.first);
      inst =
          std::make_shared<ir::Load>(dest, addresses.at(dest->getIdentifier()));
    }
  }
}

void SSADestruction::sequentializeParallelCopies() {
  // We adopt a naive approach here: we first load the old values to temporary
  // registers and rename the later use. Then, for each Copy, we simply put a
  // Store.
  // Redundant Loads can be eliminate in later passes.

  for (auto &bb : func.getMutableBBs()) {
    std::unordered_map<std::string, std::shared_ptr<ir::Reg>>
        regHoldingOldValue;
    const auto &pcopies = parallelCopies[bb.getLabelID()];
    for (auto &pcopy : pcopies) {
      auto oldVal = regHoldingOldValue[pcopy.dest->getIdentifier()] =
          func.makeTempLocalReg();
      auto addr = addresses.at(pcopy.dest->getIdentifier());
      bb.appendInstBeforeTerminator(std::make_shared<ir::Load>(oldVal, addr));
      std::shared_ptr<ir::Addr> newVal = pcopy.val;
      if (auto p = ir::dycLocalReg(pcopy.val)) {
        auto iter = regHoldingOldValue.find(p->getIdentifier());
        if (iter != regHoldingOldValue.end())
          newVal = iter->second;
      }
      bb.appendInstBeforeTerminator(std::make_shared<ir::Store>(addr, newVal));
    }
  }
}

} // namespace mocker
