#include "global_value_numbering.h"

#include <algorithm>
#include <cassert>

#include "helper.h"
#include "ir/helper.h"
#include "ir/ir_inst.h"
#include "set_operation.h"
#include "small_map.h"

//#include <iostream>

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

InstHash::Key InstHash::operator()(const std::shared_ptr<ir::IRInst> &inst) {
  if (auto p = ir::dyc<ir::Assign>(inst)) {
    auto &entry = history[p->getDest()] = getOrSet(p->getOperand());
    return entry.toString();
  }

  if (auto p = ir::dyc<ir::ArithUnaryInst>(inst)) {
    return hashArithUnary(p);
  }

  if (auto p = ir::dyc<ir::ArithBinaryInst>(inst)) {
    return hashArithBinary(p);
  }

  if (auto p = ir::dyc<ir::RelationInst>(inst)) {
    return hashRelation(p);
  }
  assert(false);
}

InstHash::Key
InstHash::hashArithUnary(const std::shared_ptr<ir::ArithUnaryInst> &inst) {
  auto dest = inst->getDest();
  auto &entry = history[dest];
  auto operand = inst->getOperand();
  auto operandEntry = getOrSet(operand);

  if (auto litP = ir::dyc<ir::IntLiteral>(operand)) {
    auto lit = litP->getVal();
    entry.clear();
    if (inst->getOp() == ir::ArithUnaryInst::Neg)
      lit = -lit;
    else
      lit = ~(std::uint64_t)lit;
    entry.addLit(lit);
    return entry.toString();
  }

  auto reg = ir::dyc<ir::Reg>(operand);
  if (inst->getOp() == ir::ArithUnaryInst::BitNot) {
    entry.update(dest);
    return "~" + operandEntry.toString();
  }

  // Neg
  entry = operandEntry;
  entry.neg();
  return entry.toString();
}

InstHash::Key
InstHash::hashArithBinary(const std::shared_ptr<ir::ArithBinaryInst> &inst) {
  if (inst->getOp() == ir::ArithBinaryInst::Add ||
      inst->getOp() == ir::ArithBinaryInst::Sub)
    return hashAddSub(inst);

  using namespace std::string_literals;
  static const SmallMap<ir::ArithBinaryInst::OpType, std::string> names{
      {ir::ArithBinaryInst::BitOr, "|"s}, {ir::ArithBinaryInst::BitAnd, "&"s},
      {ir::ArithBinaryInst::Xor, "^"s},   {ir::ArithBinaryInst::Shl, "<<"s},
      {ir::ArithBinaryInst::Shr, ">>"s},  {ir::ArithBinaryInst::Mul, "*"s},
      {ir::ArithBinaryInst::Div, "/"s},   {ir::ArithBinaryInst::Mod, "%"s}};
  static const SmallMap<ir::ArithBinaryInst::OpType, int> commutative{
      {ir::ArithBinaryInst::BitOr, 0}, {ir::ArithBinaryInst::BitAnd, 0},
      {ir::ArithBinaryInst::Xor, 0},   {ir::ArithBinaryInst::Add, 0},
      {ir::ArithBinaryInst::Mul, 0},
  };

  auto op = inst->getOp();
  auto dest = inst->getDest();
  auto &entry = history[dest];
  auto lhs = inst->getLhs(), rhs = inst->getRhs();
  auto lhsEntry = getOrSet(lhs), rhsEntry = getOrSet(rhs);

  if (commutative.in(op) && lhsEntry > rhsEntry)
    std::swap(lhsEntry, rhsEntry);

  if ((op == ir::ArithBinaryInst::Mul || op == ir::ArithBinaryInst::BitAnd) &&
      ((lhsEntry.isLiteral() && !lhsEntry.getLiteral()) ||
       (rhsEntry.isLiteral() && !rhsEntry.getLiteral()))) {
    entry = History(0);
    return entry.toString();
  }

  entry.update(dest);
  return lhsEntry.toString() + names.at(op) + rhsEntry.toString();
}

InstHash::Key
InstHash::hashAddSub(const std::shared_ptr<ir::ArithBinaryInst> &inst) {
  assert(inst->getOp() == ir::ArithBinaryInst::Add ||
         inst->getOp() == ir::ArithBinaryInst::Sub);
  auto dest = inst->getDest();
  assert(!isIn(history, dest));
  auto &entry = history[dest];

  auto lhs = inst->getLhs(), rhs = inst->getRhs();
  bool isAddition = inst->getOp() == ir::ArithBinaryInst::Add;

  entry.update(lhs);
  entry.update(rhs, isAddition);
  return entry.toString();
}

InstHash::Key
InstHash::hashRelation(const std::shared_ptr<ir::RelationInst> &inst) {
  auto op = inst->getOp();
  auto dest = inst->getDest();
  auto &entry = history[dest];
  auto lhs = inst->getLhs(), rhs = inst->getRhs();
  auto lhsEntry = getOrSet(lhs), rhsEntry = getOrSet(rhs);

  if (lhsEntry == rhsEntry) {
    if (op == ir::RelationInst::Eq || op == ir::RelationInst::Le ||
        op == ir::RelationInst::Ge)
      entry = History(1);
    else
      entry = History(0);
    return entry.toString();
  }

  using namespace std::string_literals;
  static const SmallMap<ir::RelationInst::OpType, std::string> names{
      {ir::RelationInst::Eq, "=="s}, {ir::RelationInst::Ne, "!="s},
      {ir::RelationInst::Lt, "<"s},  {ir::RelationInst::Gt, ">"s},
      {ir::RelationInst::Le, "<="s}, {ir::RelationInst::Ge, ">="s}};

  if (lhsEntry > rhsEntry &&
      (op == ir::RelationInst::Eq || op == ir::RelationInst::Ne))
    std::swap(lhsEntry, rhsEntry);
  if (op == ir::RelationInst::Ge) {
    std::swap(lhsEntry, rhsEntry);
    op = ir::RelationInst::Le;
  } else if (op == ir::RelationInst::Gt) {
    std::swap(lhsEntry, rhsEntry);
    op = ir::RelationInst::Lt;
  }

  entry.update(dest);
  return lhsEntry.toString() + names.at(op) + rhsEntry.toString();
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

// If the options of this phi-functions are the same with some previous one,
// then return the destination of that phi-function, otherwise nullptr
template <class Iter>
std::shared_ptr<ir::Reg>
getReusablePhiResult(const std::shared_ptr<ir::Phi> &phi, Iter beg, Iter end) {
  auto sortOptions = [](const std::vector<ir::Phi::Option> &options) {
    auto res = options;
    std::sort(res.begin(), res.end(),
              [](const ir::Phi::Option &lhs, const ir::Phi::Option &rhs) {
                return lhs.second->getID() < rhs.second->getID();
              });
    return res;
  };

  auto sortedOptions = sortOptions(phi->getOptions());

  auto isSame = [&sortedOptions,
                 sortOptions](const std::shared_ptr<ir::Phi> &o) {
    auto oOptions = sortOptions(o->getOptions());
    if (sortedOptions.size() != oOptions.size())
      return false;
    for (std::size_t i = 0, sz = oOptions.size(); i != sz; ++i) {
      if (!areSameAddrs(sortedOptions[i].first, oOptions[i].first))
        return false;
    }
    return true;
  };

  for (auto iter = beg; iter != end; ++iter) {
    auto &inst = *iter;
    if (ir::dyc<ir::Deleted>(inst))
      continue;
    auto oPhi = ir::dyc<ir::Phi>(inst);
    assert(oPhi);
    if (isSame(oPhi))
      return oPhi->getDest();
  }
  return nullptr;
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
    if (auto val = uniquePhiValues(phi)) { // phi is meaningless
      valueNumber.set(phi->getDest(), val);
      inst = std::make_shared<ir::Deleted>();
      ++cnt;
      continue;
    }
    if (auto val =
            getReusablePhiResult(phi, bb.getMutableInsts().begin(), iter)) {
      valueNumber.set(phi->getDest(), val);
      inst = std::make_shared<ir::Deleted>();
      ++cnt;
      continue;
    }
    valueNumber.set(phi->getDest(), phi->getDest());
    // TODO: add p to the hash table ?
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

    auto key = instHash(inst);
    // std::cerr << dest->getIdentifier() << " = " << key << std::endl;
    if (isIn(exprReg, key)) {
      auto res = exprReg.at(key);
      valueNumber.set(dest, res);

      auto tmp = valueNumber.get(dest);

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
      auto phi = ir::dyc<ir::Phi>(inst);
      if (!phi)
        break;
      inst = adjustPhi(phi);
    }
  }

  for (auto c : dominatorTree.getChildren(bbLabel))
    doValueNumbering(c, valueNumber, instHash, exprReg);
}

} // namespace mocker
