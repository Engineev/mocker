#include "global_value_numbering.h"

#include <algorithm>
#include <cassert>

#include "helper.h"
#include "ir/helper.h"
#include "ir/ir_inst.h"
#include "set_operation.h"
#include "small_map.h"

#include "ir/printer.h"
#include <iostream>

namespace mocker {
namespace detail {

std::shared_ptr<ir::Addr>
ValueNumberTable::get(const std::shared_ptr<ir::Addr> &addr) {
  if (ir::dyc<ir::IntLiteral>(addr))
    return addr;
  auto reg = ir::dyc<ir::Reg>(addr);
  assert(reg);

  auto iter = vn.find(reg);
  if (iter != vn.end())
    return iter->second;
  vn[reg] = reg;
  return reg;
}

void ValueNumberTable::set(const std::shared_ptr<ir::Reg> &reg,
                           const std::shared_ptr<ir::Addr> &valueNumber) {
  auto success = vn.emplace(std::make_pair(reg, valueNumber)).second;
  assert(success);
}

namespace {

std::string hash(const std::shared_ptr<ir::Addr> &addr) {
  if (auto p = ir::dyc<ir::IntLiteral>(addr))
    return "int:" + std::to_string(p->getVal());
  if (auto p = ir::dycGlobalReg(addr))
    return "global:" + p->getIdentifier();
  if (auto p = ir::dycLocalReg(addr))
    return "local:" + p->getIdentifier();
  assert(false);
}

} // namespace

InstHash::Key InstHash::operator()(const std::shared_ptr<ir::IRInst> &inst) {
  using namespace std::string_literals;

  if (auto p = ir::dyc<ir::Assign>(inst)) {
    return hash(p->getOperand());
  }
  if (auto p = ir::dyc<ir::ArithUnaryInst>(inst)) {
    return (p->getOp() == ir::ArithUnaryInst::Neg ? "-"s : "~"s) +
           hash(p->getOperand());
  }
  if (auto p = ir::dyc<ir::ArithBinaryInst>(inst)) {
    static const SmallMap<ir::ArithBinaryInst::OpType, std::string> names{
        {ir::ArithBinaryInst::BitOr, "|"s}, {ir::ArithBinaryInst::BitAnd, "&"s},
        {ir::ArithBinaryInst::Xor, "^"s},   {ir::ArithBinaryInst::Shl, "<<"s},
        {ir::ArithBinaryInst::Shr, ">>"s},  {ir::ArithBinaryInst::Add, "+"s},
        {ir::ArithBinaryInst::Sub, "-"s},   {ir::ArithBinaryInst::Mul, "*"s},
        {ir::ArithBinaryInst::Div, "/"s},   {ir::ArithBinaryInst::Mod, "%"s}};
    static const SmallMap<ir::ArithBinaryInst::OpType, int> associative{
        {ir::ArithBinaryInst::BitOr, 0}, {ir::ArithBinaryInst::BitAnd, 0},
        {ir::ArithBinaryInst::Xor, 0},   {ir::ArithBinaryInst::Add, 0},
        {ir::ArithBinaryInst::Mul, 0},
    };
    auto lhs = hash(p->getLhs()), rhs = hash(p->getRhs());
    if (associative.in(p->getOp()) && lhs > rhs)
      std::swap(lhs, rhs);
    return names.at(p->getOp()) + lhs + "," + rhs;
  }
  if (auto p = ir::dyc<ir::RelationInst>(inst)) {
    static const SmallMap<ir::RelationInst::OpType, std::string> names{
        {ir::RelationInst::Eq, "=="s}, {ir::RelationInst::Ne, "!="s},
        {ir::RelationInst::Lt, "<"s},  {ir::RelationInst::Gt, ">"s},
        {ir::RelationInst::Le, "<="s}, {ir::RelationInst::Ge, ">="s}};
    auto op = p->getOp();
    auto lhs = hash(p->getLhs()), rhs = hash(p->getRhs());
    if (op == ir::RelationInst::Gt)
      return "<" + rhs + "," + lhs;
    if (op == ir::RelationInst::Ge)
      return "<=" + rhs + "," + lhs;

    if ((op == ir::RelationInst::Eq || op == ir::RelationInst::Ne) && lhs > rhs)
      std::swap(lhs, rhs);
    return names.at(op) + lhs + "," + rhs;
  }
  if (auto p = ir::dyc<ir::Phi>(inst)) {
    std::string res = "phi,";
    auto options = p->getOptions();
    std::sort(options.begin(), options.end(),
              [](const ir::Phi::Option &lhs, const ir::Phi::Option &rhs) {
                return lhs.second->getID() < rhs.second->getID();
              });
    for (auto &option : options)
      res += "(" + hash(option.first) + "," +
             std::to_string(option.second->getID()) + ")";
    return res;
  }
  assert(false);
}

} // namespace detail
} // namespace mocker

namespace mocker {

GlobalValueNumbering::GlobalValueNumbering(ir::FunctionModule &func)
    : FuncPass(func) {}

bool GlobalValueNumbering::operator()() {
  dominatorTree.init(func);
  detail::ValueNumberTable valueNumber;
  detail::InstHash instHash;
  ExprRegMap exprReg;
  doValueNumbering(func.getFirstBBLabel(), valueNumber, instHash, exprReg);
  removeDeletedInsts(func);
  return cnt != 0;
}

namespace {

bool areSameAddrs(const std::shared_ptr<ir::Addr> &lhs,
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

// If all values in the phi-function options are the same, return that value,
// otherwise nullptr.
std::shared_ptr<ir::Addr> uniquePhiValues(const std::shared_ptr<ir::Phi> &phi) {
  auto res = phi->getOptions().front().first;
  for (auto &option : phi->getOptions()) {
    if (!areSameAddrs(res, option.first))
      return nullptr;
  }
  return res;
}

} // namespace

void GlobalValueNumbering::doValueNumbering(
    std::size_t bbLabel, detail::ValueNumberTable valueNumber,
    detail::InstHash instHash, ExprRegMap exprReg) {
  auto &bb = func.getMutableBasicBlock(bbLabel);

  auto iter = bb.getMutableInsts().begin();
  for (;; ++iter) {
    auto &inst = *iter;
    if (ir::dyc<ir::Deleted>(inst))
      continue;
    const auto phi = ir::dyc<ir::Phi>(inst);
    if (!phi)
      break;

    if (!canProcessPhi(phi, valueNumber)) {
      valueNumber.set(phi->getDest(), phi->getDest());
      continue;
    }

    if (auto val = uniquePhiValues(phi)) { // phi is meaningless
      valueNumber.set(phi->getDest(), val);
      inst = std::make_shared<ir::Deleted>();
      ++cnt;
      continue;
    }

    auto key = instHash(inst);
    if (isIn(exprReg, key)) { // reusable
      valueNumber.set(phi->getDest(), exprReg.at(key));
      inst = std::make_shared<ir::Deleted>();
      ++cnt;
      continue;
    }

    valueNumber.set(phi->getDest(), phi->getDest());
    exprReg[key] = phi->getDest();
  }

  for (auto End = bb.getMutableInsts().end(); iter != End; ++iter) {
    auto &inst = *iter;
    auto newOperands = ir::getOperandsUsed(inst);
    for (auto &operand : newOperands)
      operand = valueNumber.get(operand);
    inst = ir::copyWithReplacedOperands(inst, newOperands);

    auto dest = ir::getDest(inst);
    if (!dest)
      continue;
    if (ir::dyc<ir::Load>(inst) || ir::dyc<ir::Alloca>(inst) ||
        ir::dyc<ir::Malloc>(inst) || ir::dyc<ir::Call>(inst)) {
      valueNumber.set(dest, dest);
      continue;
    }
    // Do not value number relation instructions.
    // [cf. instruction_selection.cpp, genRelation]
    if (ir::dyc<ir::RelationInst>(inst)) {
      valueNumber.set(dest, dest);
      continue;
    }

    auto key = instHash(inst);
    if (isIn(exprReg, key)) {
      auto res = exprReg.at(key);
      valueNumber.set(dest, res);
      inst = std::make_shared<ir::Deleted>();
      ++cnt;
      continue;
    }
    valueNumber.set(dest, dest);
    exprReg[key] = dest;
  }

  auto adjustPhi = [bbLabel,
                    &valueNumber](const std::shared_ptr<ir::Phi> &phi) {
    std::vector<ir::Phi::Option> newOptions;
    for (auto &option : phi->getOptions()) {
      if (option.second->getID() != bbLabel) {
        newOptions.emplace_back(option);
        continue;
      }
      newOptions.emplace_back(
          std::make_pair(valueNumber.get(option.first), option.second));
    }
    return std::make_shared<ir::Phi>(phi->getDest(), std::move(newOptions));
  };
  for (auto sucLabel : bb.getSuccessors()) {
    auto &sucBB = func.getMutableBasicBlock(sucLabel);
    for (auto &inst : sucBB.getMutableInsts()) {
      if (ir::dyc<ir::Deleted>(inst))
        continue;
      auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        break;
      inst = adjustPhi(phi);
    }
  }

  for (auto c : dominatorTree.getChildren(bbLabel))
    doValueNumbering(c, valueNumber, instHash, exprReg);
}

bool GlobalValueNumbering::canProcessPhi(
    const std::shared_ptr<ir::Phi> &phi,
    const detail::ValueNumberTable &vn) const {
  for (auto &option : phi->getOptions()) {
    if (!vn.has(option.first))
      return false;
  }
  return true;
}

} // namespace mocker
