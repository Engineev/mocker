#include "reassociation.h"

#include <functional>
#include <iostream>

#include "helper.h"
#include "ir/printer.h"
#include "set_operation.h"

namespace mocker {

void detail::ReassociationImpl::findRoots() {
  for (auto &inst : bb.getInsts()) {
    auto dest = ir::getDest(inst);
    if (!dest)
      continue;

    if (!ir::dyc<ir::ArithBinaryInst>(inst) &&
        !ir::dyc<ir::ArithUnaryInst>(inst) && !ir::dyc<ir::Assign>(inst))
      continue;

    if (auto binary = ir::dyc<ir::ArithBinaryInst>(inst)) {
      if (binary->getOp() != ir::ArithBinaryInst::Add &&
          binary->getOp() != ir::ArithBinaryInst::Sub)
        continue;
    }

    if (auto unary = ir::dyc<ir::ArithUnaryInst>(inst)) {
      if (unary->getOp() != ir::ArithUnaryInst::Neg)
        continue;
    }

    const auto Uses = defUse.getUses(dest);
    if (Uses.empty())
      continue;
    if (Uses.size() > 1) {
      roots.emplace(dest);
      continue;
    }
    assert(Uses.size() == 1);
    if (Uses.at(0).getBBLabel() != bb.getLabelID()) {
      roots.emplace(dest);
      continue;
    }
    auto useInst = Uses.at(0).getInst();
    // TODO
    if (ir::dyc<ir::Ret>(useInst) || ir::dyc<ir::Store>(useInst) ||
        ir::dyc<ir::Call>(useInst) || ir::dyc<ir::Load>(useInst)) {
      roots.emplace(dest);
    }
  }
}

std::shared_ptr<detail::ReassociationImpl::Node>
detail::ReassociationImpl::buildTrees(const std::shared_ptr<ir::Addr> &node,
                                      bool firstNode) {
  if (auto lit = ir::dyc<ir::IntLiteral>(node)) {
    return std::make_shared<Node>(Node::Literal, lit);
  }
  if (auto gReg = ir::dycGlobalReg(node)) {
    return std::make_shared<Node>(Node::Leaf, gReg);
  }

  auto reg = ir::dycLocalReg(node);
  assert(reg);
  if (isParameter(func, reg->getIdentifier()) ||
      reg->getIdentifier() == ".phi_nan") {
    return std::make_shared<Node>(Node::Leaf, reg);
  }

  if (!firstNode && isIn(roots, reg)) {
    return std::make_shared<Node>(Node::Root, reg);
  }

  auto def = useDef.getDef(reg);
  if (def.getBBLabel() != bb.getLabelID()) {
    return std::make_shared<Node>(Node::Leaf, reg);
  }

  auto inst = def.getInst();
  if (auto binary = ir::dyc<ir::ArithBinaryInst>(inst)) {
    if (binary->getOp() != ir::ArithBinaryInst::Add &&
        binary->getOp() != ir::ArithBinaryInst::Sub) {
      return std::make_shared<Node>(Node::Leaf, reg);
    }
    auto res = std::make_shared<Node>(
        binary->getOp() == ir::ArithBinaryInst::Add ? Node::Add : Node::Sub,
        reg);
    res->children.emplace_back(buildTrees(binary->getLhs(), false));
    res->children.emplace_back(buildTrees(binary->getRhs(), false));
    return res;
  }

  if (auto unary = ir::dyc<ir::ArithUnaryInst>(inst)) {
    if (unary->getOp() != ir::ArithUnaryInst::Neg)
      return std::make_shared<Node>(Node::Leaf, reg);
    auto res = std::make_shared<Node>(Node::Neg, reg);
    res->children.emplace_back(buildTrees(unary->getOperand(), false));
    return res;
  }

  if (auto assign = ir::dyc<ir::Assign>(inst)) {
    auto res = std::make_shared<Node>(Node::Assign, reg);
    res->children.emplace_back(buildTrees(assign->getOperand(), false));
    return res;
  }

  return std::make_shared<Node>(Node::Leaf, reg);
}

std::vector<std::pair<bool, std::shared_ptr<ir::Addr>>>
detail::ReassociationImpl::flatten(
    const std::shared_ptr<detail::ReassociationImpl::Node> &root) {
  std::vector<std::pair<bool, std::shared_ptr<ir::Addr>>> res;

  std::function<void(const std::shared_ptr<Node>, bool)> impl =
      [&res, &impl](const std::shared_ptr<Node> &node, bool positive) {
        if (node->children.empty()) {
          res.emplace_back(positive, node->value);
          return;
        }

        if (node->type == Node::Add) {
          impl(node->children[0], positive);
          impl(node->children[1], positive);
          return;
        }
        if (node->type == Node::Sub) {
          impl(node->children[0], positive);
          impl(node->children[1], 1 - positive);
          return;
        }
        if (node->type == Node::Neg) {
          impl(node->children[0], 1 - positive);
          return;
        }
        if (node->type == Node::Assign) {
          impl(node->children[0], positive);
          return;
        }
        std::terminate();
      };

  impl(root, true);

  return res;
}

bool detail::ReassociationImpl::areSameAddrs(
    const std::shared_ptr<ir::Addr> &lhs,
    const std::shared_ptr<ir::Addr> &rhs) {
  auto lhsReg = ir::dyc<ir::Reg>(lhs);
  auto lhsLit = ir::dyc<ir::IntLiteral>(lhs);
  assert(lhsReg || lhsLit);
  auto rhsReg = ir::dyc<ir::Reg>(rhs);
  auto rhsLit = ir::dyc<ir::IntLiteral>(rhs);
  assert(rhsReg || rhsLit);
  if (lhsLit && rhsLit)
    return lhsLit->getVal() == rhsLit->getVal();
  if (lhsReg && rhsReg)
    return lhsReg->getIdentifier() == rhsReg->getIdentifier();
  return false;
}

std::vector<std::pair<bool, std::shared_ptr<ir::Addr>>>
detail::ReassociationImpl::cancel(
    const std::vector<std::pair<bool, std::shared_ptr<ir::Addr>>> &nodes) {
  std::vector<std::pair<bool, std::shared_ptr<ir::Addr>>> res;
  std::int64_t literal = 0;
  std::list<std::shared_ptr<ir::Addr>> positive;
  std::list<std::shared_ptr<ir::Addr>> negative;

  for (auto &node : nodes) {
    if (auto lit = ir::dyc<ir::IntLiteral>(node.second)) {
      literal += node.first ? lit->getVal() : -lit->getVal();
      continue;
    }
    if (node.first)
      positive.emplace_back(node.second);
    else
      negative.emplace_back(node.second);
  }

  for (auto &p : positive) {
    auto iter = negative.begin();
    for (auto end = negative.end(); iter != end; ++iter) {
      if (areSameAddrs(p, *iter))
        break;
    }
    if (iter != negative.end()) {
      negative.erase(iter);
      continue;
    }
    res.emplace_back(true, p);
  }

  for (auto &n : negative)
    res.emplace_back(false, n);
  res.emplace_back(true, std::make_shared<ir::IntLiteral>(literal));
  return res;
}

bool detail::ReassociationImpl::operator()() {
  findRoots();
  for (auto &root : roots) {
    trees[root] = buildTrees(root, true);
  }
  for (auto &kv : trees) {
    auto nodes = flatten(kv.second);
    if (nodes.size() < 10)
      doNotRebuild.emplace(kv.first);
    flattenedNodes[kv.first] = cancel(nodes);
  }

  for (auto &root : roots) {
    rankNodes(root);
  }

  for (auto &root : roots) {
    rebuild(root);
  }

  removeDeletedInsts(func);

  return false;
}

void detail::ReassociationImpl::rankNodes(
    const std::shared_ptr<ir::Reg> &root) {
  if (isIn(rankedNodes, root))
    return;
  auto &res = rankedNodes[root];

  int rank = 0;
  auto &nodes = flattenedNodes.at(root);
  for (auto &node : nodes) {
    if (auto lit = ir::dyc<ir::IntLiteral>(node.second)) {
      res.emplace(true,
                  node.first ? lit
                             : std::make_shared<ir::IntLiteral>(-lit->getVal()),
                  0);
      continue;
    }
    auto reg = ir::dyc<ir::Reg>(node.second);
    if (isIn(roots, reg)) {
      if (!isIn(rootRank, reg))
        rankNodes(reg);
      rank += rootRank.at(reg);
      res.emplace(node.first, reg, rootRank.at(reg));
      continue;
    }

    // is leaf
    res.emplace(node.first, reg, 1);
    rank++;
  }

  rootRank[root] = rank;
}

void detail::ReassociationImpl::rebuild(const std::shared_ptr<ir::Reg> &root) {
  if (isIn(rebuilt, root) || isIn(doNotRebuild, root))
    return;

  auto pos = bb.getMutableInsts().begin();
  for (auto end = bb.getMutableInsts().end(); pos != end; ++pos) {
    auto dest = ir::getDest(*pos);
    if (!dest)
      continue;
    if (dest->getIdentifier() == root->getIdentifier())
      break;
  }
  assert(pos != bb.getMutableInsts().end());
  rebuild(rankedNodes.at(root), pos);
  *pos = std::make_shared<ir::Deleted>();
}

void detail::ReassociationImpl::rebuild(
    std::priority_queue<detail::ReassociationImpl::RankedNode> &q,
    std::list<std::shared_ptr<ir::IRInst>>::iterator pos) {
  auto &insts = bb.getMutableInsts();
  auto dest = ir::getDest(*pos);

  //  std::cerr << "rebuilding: " <<  ir::fmtAddr(dest) << std::endl;

  if (q.size() == 1) {
    auto val = q.top();
    if (val.positive)
      insts.insert(pos, std::make_shared<ir::Assign>(dest, val.value));
    else
      insts.insert(pos, std::make_shared<ir::ArithUnaryInst>(
                            dest, ir::ArithUnaryInst::Neg, val.value));
    return;
  }

  while (!q.empty()) {
    dest = ir::getDest(*pos);
    auto lhs = q.top();
    q.pop();
    if (q.empty())
      return;
    auto rhs = q.top();
    q.pop();

    assert(!ir::dyc<ir::IntLiteral>(lhs.value) ||
           !ir::dyc<ir::IntLiteral>(rhs.value));

    if (!q.empty())
      dest = func.makeTempLocalReg();

    // emit
    if (!lhs.positive)
      std::swap(lhs, rhs);
    auto lhsVal = lhs.value;
    if (!lhs.positive) {
      lhsVal = func.makeTempLocalReg();
      auto newInst = std::make_shared<ir::ArithUnaryInst>(
          ir::dycLocalReg(lhsVal), ir::ArithUnaryInst::Neg, lhs.value);
      insts.insert(pos, newInst);
    }
    auto newInst = std::make_shared<ir::ArithBinaryInst>(
        dest,
        rhs.positive ? ir::ArithBinaryInst::Add : ir::ArithBinaryInst::Sub,
        lhsVal, rhs.value);
    insts.insert(pos, newInst);
    if (!q.empty()) {
      q.emplace(true, dest, lhs.rank + rhs.rank);
    }
  }
}

} // namespace mocker
