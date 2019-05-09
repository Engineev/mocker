#include "induction_variable.h"

#include <cassert>
#include <utility>

#include "helper.h"
#include "set_operation.h"

#include "ir/printer.h"
#include <iostream>

namespace mocker {

bool InductionVariable::isInductionVariable(
    const std::shared_ptr<ir::ArithBinaryInst> &defInst,
    const ir::RegSet &curIVs, const ir::RegSet &loopInv) const {
  if (defInst->getOp() == ir::ArithBinaryInst::Sub) {
    auto lhsReg = ir::dycLocalReg(defInst->getLhs());
    if (!lhsReg || !isIn(curIVs, lhsReg))
      return false;
    return ir::dyc<ir::IntLiteral>(defInst->getRhs()) ||
           isIn(loopInv, ir::dyc<ir::Reg>(defInst->getRhs()));
  }

  if (defInst->getOp() == ir::ArithBinaryInst::Add ||
      defInst->getOp() == ir::ArithBinaryInst::Mul) {
    auto lhs = defInst->getLhs(), rhs = defInst->getRhs();
    if (ir::dyc<ir::IntLiteral>(lhs) || !isIn(curIVs, ir::dyc<ir::Reg>(lhs)))
      std::swap(lhs, rhs);
    auto lhsReg = ir::dyc<ir::Reg>(lhs);
    if (!lhsReg || !isIn(curIVs, lhsReg))
      return false;
    return ir::dyc<ir::IntLiteral>(defInst->getRhs()) ||
           isIn(loopInv, ir::dyc<ir::Reg>(defInst->getRhs()));
  }

  return false;
}
bool InductionVariable::isInductionVariable(
    const std::shared_ptr<ir::Phi> &defInst, const ir::RegSet &loopInv) const {
  for (auto &option : defInst->getOptions()) {
    auto reg = ir::dyc<ir::Reg>(option.first);
    if (!reg)
      continue;
    if (!isIn(loopInv, reg))
      return false;
  }
  return true;
}

bool InductionVariable::operator()() {
  loopInfo.init(func, funcAttr);
  useDef.init(func);
  defUse.init(func);
  auto loopHeads = loopInfo.postOrder();
  for (auto header : loopHeads) {
    if (header == func.getFirstBBLabel())
      continue;
    processLoop(header);
  }
  return false;
}

namespace {

// return the initial value and the value from the loop
std::pair<std::shared_ptr<ir::Addr>, std::shared_ptr<ir::Addr>>
getInitAndLoopVal(const std::shared_ptr<ir::Phi> &phi,
                  const std::unordered_set<std::size_t> &loopNodes) {
  assert(phi->getOptions().size() == 2);
  auto out = phi->getOptions().at(0).second->getID();
  auto in = phi->getOptions().at(1).second->getID();
  auto init = phi->getOptions().at(0).first;
  auto loopVal = phi->getOptions().at(1).first;
  ;
  if (isIn(loopNodes, out)) {
    std::swap(in, out);
    std::swap(init, loopVal);
  }
  if (!isIn(loopNodes, out) && isIn(loopNodes, in))
    return {init, loopVal};
  return {nullptr, nullptr};
}

} // namespace

ir::RegMap<mocker::InductionVariable::IVar>
InductionVariable::findCandidatePhis(std::size_t header) {
  ir::RegMap<mocker::InductionVariable::IVar> res;
  auto &bb = func.getBasicBlock(header);
  const auto &loopNodes = loopInfo.getLoops().at(header);
  const auto &loopInv = loopInfo.getLoopInvariantVariables(header);

  for (auto &inst : bb.getInsts()) {
    auto phi = ir::dyc<ir::Phi>(inst);
    if (!phi)
      break;
    if (phi->getOptions().size() != 2)
      continue;
    auto initAndLoopVal = getInitAndLoopVal(phi, loopNodes);
    if (!initAndLoopVal.first)
      continue;
    auto loopVal = ir::dycLocalReg(initAndLoopVal.second);
    if (!loopVal)
      continue;
    auto def = useDef.getDef(loopVal);
    assert(isIn(loopNodes, def.getBBLabel()));
    auto binary = ir::dyc<ir::ArithBinaryInst>(def.getInst());
    if (!binary || binary->getOp() != ir::ArithBinaryInst::Add)
      continue;
    auto lhs = binary->getLhs(), rhs = binary->getRhs();
    if (!ir::dyc<ir::IntLiteral>(rhs) && !isIn(loopInv, ir::dyc<ir::Reg>(rhs)))
      std::swap(lhs, rhs);
    if (!ir::dyc<ir::IntLiteral>(rhs) && !isIn(loopInv, ir::dyc<ir::Reg>(rhs)))
      continue;
    // rhs is loop invariant
    if (!areSameAddrs(lhs, phi->getDest()))
      continue;
    res[phi->getDest()] = IVar(initAndLoopVal.first, rhs);
  }

  return res;
}

void InductionVariable::processLoop(std::size_t header) {
  // First, we find all phi functions of form:
  //   x = phi (x1, b1), (x2, b2)
  // where b1 is a node outside the loop and b2 inside. Then we check whether
  // x2 is induction variable. We call such phi functions candidate phi
  // functions.
  auto candidates = findCandidatePhis(header);
  // Now we check if we can reuse the result of some phi functions. Namely,
  // does there exist some candidate phi functions whose initial value and
  // step are the same as some other phi functions.
  auto tryReusing =
      [&candidates](
          ir::RegMap<mocker::InductionVariable::IVar>::const_iterator end)
      -> std::shared_ptr<ir::Reg> {
    auto &curIV = end->second;
    for (auto iter = candidates.begin(); iter != end; ++iter) {
      auto &iv = iter->second;
      if (areSameAddrs(curIV.initial, iv.initial) &&
          areSameAddrs(curIV.step, iv.step)) {
        return iter->first;
      }
    }
    return nullptr;
  };

  ir::RegMap<std::shared_ptr<ir::Reg>> newName;
  for (auto iter = candidates.begin(), end = candidates.end(); iter != end;
       ++iter) {
    auto reuse = tryReusing(iter);
    if (!reuse)
      continue;
    newName[iter->first] = reuse;
  }

//  std::cerr << "reusing: ";
//  for (auto &kv : newName) {
//    std::cerr << ir::fmtAddr(kv.first) << " <- " << ir::fmtAddr(kv.second)
//              << ", ";
//  }
//  std::cerr << std::endl;

  // Finally, we replace the phi functions by assignment
  auto &bb = func.getMutableBasicBlock(header);
  auto pos = bb.getMutableInsts().begin();
  for (;; ++pos) {
    if (!ir::dyc<ir::Phi>(*pos))
      break;
  }
  std::vector<std::shared_ptr<ir::Assign>> toBeInserted;
  for (auto &inst : bb.getMutableInsts()) {
    auto phi = ir::dyc<ir::Phi>(inst);
    if (!phi)
      break;
    if (!isIn(newName, phi->getDest()))
      continue;
    auto reuse = newName.at(phi->getDest());
    toBeInserted.emplace_back(
        std::make_shared<ir::Assign>(phi->getDest(), reuse));
    inst = std::make_shared<ir::Deleted>();
  }

  for (auto &assign : toBeInserted)
    bb.getMutableInsts().insert(pos, assign);

  removeDeletedInsts(func);
}

} // namespace mocker
