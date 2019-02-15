#include "ssa.h"

#include <cassert>
#include <functional>
#include <queue>

namespace mocker {

ConstructSSA::ConstructSSA(ir::FunctionModule &func) : FuncPass(func) {}

void ConstructSSA::operator()() {
  computeAuxiliaryInfo();
  insertPhiFunctions();
  renameVariables();
}

void ConstructSSA::computeAuxiliaryInfo() {
  buildDominating();
  buildDominators();
  buildImmediateDominator();
  buildDominatorTree();
  buildDominatingFrontier();
}

void ConstructSSA::buildDominating() {
  for (const auto &bb : func.getBBs())
    buildDominatingImpl(bb.getLabelID());
}

void ConstructSSA::buildDominatingImpl(std::size_t node) {
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

bool ConstructSSA::isDominating(std::size_t u, std::size_t v) const {
  const auto &d = bbInfo.at(u).dominating;
  return d.find(v) != d.end();
}

bool ConstructSSA::isStrictlyDominating(std::size_t u, std::size_t v) const {
  return u != v && isDominating(u, v);
}

void ConstructSSA::buildDominators() {
  for (const auto &infoPair : bbInfo) {
    auto u = infoPair.first;
    const auto &info = infoPair.second;
    for (auto dominatedByU : info.dominating)
      bbInfo[dominatedByU].dominators.emplace(u);
  }
}

void ConstructSSA::buildImmediateDominator() {
  // For a faster algorithm, see [Lengauer, Tarjan, 1979]

  for (const auto &bb : func.getBBs())
    if (bb.getLabelID() != func.getFirstBB()->getLabelID())
      buildImmediateDominatorImpl(bb.getLabelID());
}

void ConstructSSA::buildImmediateDominatorImpl(std::size_t node) {
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

void ConstructSSA::buildDominatorTree() {
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

void ConstructSSA::buildDominatingFrontier() {
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

void ConstructSSA::insertPhiFunctions() {
  const auto &firstBB = *func.getFirstBB();
  for (const auto &inst : firstBB.getInsts()) {
    if (auto p = std::dynamic_pointer_cast<ir::Alloca>(inst)) {
      auto reg = std::dynamic_pointer_cast<ir::LocalReg>(p->dest);
      assert(reg);
      varNames.emplace(reg->identifier);
    }
  }

  for (const auto &v : varNames)
    insertPhiFunctions(v);
}

void ConstructSSA::insertPhiFunctions(const std::string &varName) {
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

      auto dest = func.makeTempLocalReg();
      auto phi =
          std::make_shared<ir::Phi>(dest, std::vector<ir::Phi::Option>());
      varDefined[phi->getID()] = varName;
      bbDefined[dest->identifier] = frontierBB;
      auto &bbInst = func.getMutableBasicBlock(frontierBB);
      bbInst.getMutablePhis().emplace_back(phi);

      added.emplace(frontierBB);
      if (!isIn(frontierBB, originalDefs))
        remaining.emplace(frontierBB, dest);
    }
  }
}

std::vector<ConstructSSA::Definition>
ConstructSSA::collectAndReplaceDefs(const std::string &name) {
  std::vector<Definition> res;
  for (auto &bb : func.getMutableBBs()) {
    for (auto &inst : bb.getMutableInsts()) {
      auto p = std::dynamic_pointer_cast<ir::Store>(inst);
      if (!p)
        continue;
      auto reg = std::dynamic_pointer_cast<ir::LocalReg>(p->dest);
      if (!reg)
        continue;
      if (reg->identifier != name)
        continue;
      auto dest = func.makeTempLocalReg();
      res.emplace_back(bb.getLabelID(), reg);
      auto assign = std::make_shared<ir::Assign>(dest, p->operand);
      inst = assign;
      varDefined[assign->getID()] = reg->identifier;
      bbDefined[dest->identifier] = bb.getLabelID();
    }
  }
  return res;
}

void ConstructSSA::renameVariables() {
  for (const auto &name : varNames)
    reachingDef[name] = ".phi_nan";
  bbDefined[".phi_nan"] = func.getFirstBB()->getLabelID();

  renameVariablesImpl(root);
}

void ConstructSSA::renameVariablesImpl(
    const std::shared_ptr<ConstructSSA::DTNode> &curNode) {
  auto &bb = curNode->content.get();
  for (auto &p : bb.getMutablePhis()) {
    auto iter = varDefined.find(p->getID());
    if (iter == varDefined.end())
      continue;
    auto varName = iter->second;
    updateReachingDef(varName, bb.getLabelID());

    auto newVarName =
        std::dynamic_pointer_cast<ir::LocalReg>(p->dest)->identifier;
    reachingDef[newVarName] = reachingDef[varName];
    reachingDef[varName] = newVarName;
  }

  for (auto &inst : bb.getMutableInsts()) {
    if (auto p = std::dynamic_pointer_cast<ir::Load>(inst)) {
      auto var = std::dynamic_pointer_cast<ir::LocalReg>(p->addr);
      if (!var) // is a global variable
        continue;
      if (!isIn(var->identifier, varNames))
        continue;
      updateReachingDef(var->identifier, bb.getLabelID());
      inst = std::make_shared<ir::Assign>(
          p->dest,
          std::make_shared<ir::LocalReg>(reachingDef.at(var->identifier)));
      continue;
    }
    if (auto p = std::dynamic_pointer_cast<ir::Assign>(inst)) {
      auto iter = varDefined.find(p->getID());
      if (iter == varDefined.end())
        continue;
      auto varName = iter->second;
      updateReachingDef(varName, bb.getLabelID());

      auto newVarName =
          std::dynamic_pointer_cast<ir::LocalReg>(p->dest)->identifier;
      reachingDef[newVarName] = reachingDef[varName];
      reachingDef[varName] = newVarName;
    }
  }

  for (const auto &sucLabel : bb.getSuccessors()) {
    auto sucBB = func.getMutableBasicBlock(sucLabel);
    for (auto &phi : sucBB.getMutablePhis()) {
      auto iter = varDefined.find(phi->getID());
      if (iter == varDefined.end())
        continue;
      auto varName = iter->second;
      phi->options.emplace_back(
          std::make_shared<ir::LocalReg>(reachingDef.at(varName)),
          std::make_shared<ir::Label>(bb.getLabelID()));
    }
  }

  for (const auto &child : curNode->children)
    renameVariablesImpl(child);
}

void ConstructSSA::updateReachingDef(const std::string &varName,
                                     std::size_t label) {
  if (reachingDef.find(varName) == reachingDef.end())
    return;

  auto r = reachingDef.at(varName);
  while (!isDominating(bbDefined.at(r), label))
    r = reachingDef.at(r);
  reachingDef.at(varName) = r;
}

} // namespace mocker
