#include "value_numbering.h"

#include <algorithm>
#include <cassert>
#include <iostream>

#include "ir/helper.h"
#include "small_map.h"

#include "ir/printer.h"

namespace mocker {

LocalValueNumbering::LocalValueNumbering(ir::BasicBlock &bb)
    : BasicBlockPass(bb) {}

bool LocalValueNumbering::operator()() {
  std::size_t cnt = 0;
  for (auto &inst : bb.getMutableInsts()) {
    auto dest = ir::getDest(inst);
    if (!dest)
      continue;
    if (ir::dyc<ir::Load>(inst) || ir::dyc<ir::AllocVar>(inst) ||
        ir::dyc<ir::Malloc>(inst) || ir::dyc<ir::Call>(inst))
      continue;
    auto operands = ir::getOperandsUsed(inst);
    std::vector<std::size_t> valueNumbers;
    valueNumbers.reserve(operands.size());
    for (auto &operand : operands) {
      makeSureDefined(operand);
      valueNumbers.emplace_back(key2ValueNumber.at(hash(operand)));
    }
    auto rhsKey = hash(inst, valueNumbers);
    auto lhsKey = hash(dest);

    auto iter = key2ValueNumber.find(rhsKey);
    if (iter != key2ValueNumber.end()) {
      key2ValueNumber[lhsKey] = iter->second;
      auto rhsVal = valueNumber2Addr.at(iter->second);
      if (auto p = ir::dyc<ir::LocalReg>(rhsVal)) {
        if (p->identifier == ir::getLocalRegIdentifier(dest))
          assert(false);
      }

      inst = std::make_shared<ir::Assign>(copy(dest), rhsVal);
      ++cnt;
      continue;
    }
    auto newValueNumber = valueNumber2Addr.size();
    valueNumber2Addr.emplace_back(ir::copy(dest));
    key2ValueNumber[lhsKey] = key2ValueNumber[rhsKey] = newValueNumber;
  }

  //    std::cerr << "LVN: modified " << cnt << " insts\n";
  return cnt != 0;
}

std::string
LocalValueNumbering::hash(const std::shared_ptr<const ir::Addr> &addr) {
  if (auto p = ir::cdyc<ir::IntLiteral>(addr))
    return "int:" + std::to_string(p->val);
  if (auto p = ir::cdyc<ir::GlobalReg>(addr))
    return "global:" + p->identifier;
  if (auto p = ir::cdyc<ir::LocalReg>(addr))
    return "local:" + p->identifier;
  assert(false);
}

void LocalValueNumbering::makeSureDefined(
    const std::shared_ptr<const ir::Addr> &addr) {
  auto key = hash(addr);
  auto iter = key2ValueNumber.find(key);
  if (iter != key2ValueNumber.end())
    return;
  auto newValueNumber = valueNumber2Addr.size();
  valueNumber2Addr.emplace_back(ir::copy(addr));
  key2ValueNumber[key] = newValueNumber;
}

std::string
LocalValueNumbering::hash(const std::shared_ptr<ir::IRInst> &inst,
                          const std::vector<std::size_t> &valueNumbers) {
  using namespace std::string_literals;
  std::vector<std::string> vnStr;
  vnStr.reserve(valueNumbers.size());
  for (auto valueNumber : valueNumbers)
    vnStr.emplace_back(std::to_string(valueNumber));

  if (auto p = ir::dyc<ir::Assign>(inst)) {
    return hash(p->getOperand());
  }
  if (auto p = ir::dyc<ir::ArithUnaryInst>(inst)) {
    return (p->getOp() == ir::ArithUnaryInst::Neg ? "-"s : "~"s) + vnStr.at(0);
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
    assert(vnStr.size() == 2);
    if (associative.in(p->getOp()) && valueNumbers[0] > valueNumbers[1])
      std::swap(vnStr[0], vnStr[1]);
    return names.at(p->getOp()) + vnStr.at(0) + "," + vnStr.at(1);
  }
  if (auto p = ir::dyc<ir::RelationInst>(inst)) {
    static const SmallMap<ir::RelationInst::OpType, std::string> names{
        {ir::RelationInst::Eq, "=="s}, {ir::RelationInst::Ne, "!="s},
        {ir::RelationInst::Lt, "<"s},  {ir::RelationInst::Gt, ">"s},
        {ir::RelationInst::Le, "<="s}, {ir::RelationInst::Ge, ">="s}};
    assert(vnStr.size() == 2);
    auto op = p->getOp();
    if (op == ir::RelationInst::Gt)
      return "<" + vnStr[1] + "," + vnStr[0];
    if (op == ir::RelationInst::Ge)
      return "<=" + vnStr[1] + "," + vnStr[0];

    if ((op == ir::RelationInst::Eq || op == ir::RelationInst::Ne) &&
        valueNumbers[0] > valueNumbers[1])
      std::swap(vnStr[0], vnStr[1]);
    return names.at(op) + vnStr[0] + "," + vnStr[1];
  }
  if (auto p = ir::dyc<ir::Phi>(inst)) {
    std::string res = "phi,";
    for (auto &s : vnStr)
      res += s + ",";
    return res;
  }
  assert(false);
}

} // namespace mocker