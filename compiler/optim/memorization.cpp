#include "memorization.h"

#include <iostream>

#include "helper.h"
#include "ir/helper.h"

namespace mocker {

bool Memorization::operator()() {
  if (!check())
    return false;
  std::cerr << "Memorization: " << func.getIdentifier() << std::endl;
  rewrite();
  removeDeletedInsts(func);
  return false;
}

void Memorization::rewrite() {
  auto setTableAddr =
      std::make_shared<ir::Reg>("@_set_table_" + func.getIdentifier());
  auto valTableAddr =
      std::make_shared<ir::Reg>("@_val_table_" + func.getIdentifier());
  module.addGlobalVar(valTableAddr->getIdentifier(), std::string(512, 0));
  module.addGlobalVar(setTableAddr->getIdentifier(), std::string(512, 0));

  auto &normalBB = *func.getFirstBB();
  auto &begBB = *func.pushBackBB();
  auto &checkSetBB = *func.pushBackBB();
  auto &cachedBB = *func.pushBackBB();

  for (auto &inst : normalBB.getMutableInsts()) {
    auto alloca = ir::dyc<ir::Alloca>(inst);
    if (!alloca)
      break;
    begBB.appendInst(alloca);
    inst = std::make_shared<ir::Deleted>();
  }

  // compute the addresses
  auto arg = std::make_shared<ir::Reg>("0");
  auto offset = func.makeTempLocalReg();
  begBB.appendInst(std::make_shared<ir::ArithBinaryInst>(
      offset, ir::ArithBinaryInst::Mul, arg,
      std::make_shared<ir::IntLiteral>(8)));
  auto setAddr = func.makeTempLocalReg();
  begBB.appendInst(std::make_shared<ir::ArithBinaryInst>(
      setAddr, ir::ArithBinaryInst::Add, offset, setTableAddr));
  auto cachedResAddr = func.makeTempLocalReg();
  begBB.appendInst(std::make_shared<ir::ArithBinaryInst>(
      cachedResAddr, ir::ArithBinaryInst::Add, offset, valTableAddr));

  auto cmpResLt = func.makeTempLocalReg();
  begBB.appendInst(
      std::make_shared<ir::RelationInst>(cmpResLt, ir::RelationInst::Lt, arg,
                                         std::make_shared<ir::IntLiteral>(64)));
  auto cmpResGe = func.makeTempLocalReg();
  begBB.appendInst(
      std::make_shared<ir::RelationInst>(cmpResGe, ir::RelationInst::Ge, arg,
                                         std::make_shared<ir::IntLiteral>(0)));
  auto cmpRes = func.makeTempLocalReg();
  begBB.appendInst(std::make_shared<ir::ArithBinaryInst>(
      cmpRes, ir::ArithBinaryInst::BitAnd, cmpResLt, cmpResGe));
  begBB.appendInst(std::make_shared<ir::Branch>(
      cmpRes, std::make_shared<ir::Label>(checkSetBB.getLabelID()),
      std::make_shared<ir::Label>(normalBB.getLabelID())));

  auto isSet = func.makeTempLocalReg();
  checkSetBB.appendInst(std::make_shared<ir::Load>(isSet, setAddr));
  checkSetBB.appendInst(std::make_shared<ir::Branch>(
      isSet, std::make_shared<ir::Label>(cachedBB.getLabelID()),
      std::make_shared<ir::Label>(normalBB.getLabelID())));

  auto cachedRes = func.makeTempLocalReg();
  cachedBB.appendInst(std::make_shared<ir::Load>(cachedRes, cachedResAddr));
  cachedBB.appendInst(std::make_shared<ir::Ret>(cachedRes));

  std::vector<std::size_t> bbs;
  for (auto &bb : func.getBBs()) {
    if (bb.getLabelID() == cachedBB.getLabelID())
      continue;
    bbs.emplace_back(bb.getLabelID());
  }
  func.buildContext();
  for (auto &bbLabel : bbs) {
    auto &bb = func.getMutableBasicBlock(bbLabel);
    auto ret = ir::dyc<ir::Ret>(bb.getInsts().back());
    if (!ret)
      continue;

    auto &noSetBB = *func.pushBackBB();
    noSetBB.appendInst(ret);

    auto &setBB = *func.pushBackBB();
    setBB.appendInst(std::make_shared<ir::Store>(
        setAddr, std::make_shared<ir::IntLiteral>(1)));
    setBB.appendInst(std::make_shared<ir::Store>(cachedResAddr, ret->getVal()));
    setBB.appendInst(ret);

    bb.getMutableInsts().back() = std::make_shared<ir::Branch>(
        cmpRes, std::make_shared<ir::Label>(setBB.getLabelID()),
        std::make_shared<ir::Label>(noSetBB.getLabelID()));
  }
}

bool Memorization::check() const {
  if (!funcAttr.isPure(func.getIdentifier()))
    return false;

  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto ret = ir::dyc<ir::Ret>(inst);
      if (!ret)
        continue;
      if (!ret->getVal())
        return false;
    }
  }

  if (func.getArgs().size() != 1)
    return false;

  // is recursive ?
  for (auto &bb : func.getBBs()) {
    for (auto &inst : bb.getInsts()) {
      auto call = ir::dyc<ir::Call>(inst);
      if (!call)
        continue;
      if (call->getFuncName() == func.getIdentifier())
        return true;
    }
  }

  return false;
}

} // namespace mocker
