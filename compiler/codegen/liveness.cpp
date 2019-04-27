#include "liveness.h"

#include "nasm/helper.h"

namespace mocker {
namespace {

using nasm::RegMap;
using nasm::RegSet;

// UEVar contains the upward-exposed variables, namely, the ones that are used
// in this block before any redefinition.
// VarKill contains all variables defined in this block.
std::pair<RegSet, RegSet> buildUEVarAndVarKill(const NasmCfg::Node &node) {
  RegSet ueVar, varKill;
  LineIter bbBeg, bbEnd;
  std::tie(bbBeg, bbEnd) = node.getLines();
  for (auto iter = bbBeg; iter != bbEnd; ++iter) {
    if (!iter->inst)
      continue;
    auto used = nasm::getUsedRegs(iter->inst);
    for (auto &reg : used) {
      if (varKill.find(reg) == varKill.end())
        ueVar.emplace(reg);
    }
    auto defined = nasm::getDefinedRegs(iter->inst);
    for (auto &reg : defined)
      varKill.emplace(reg);
  }
  return {ueVar, varKill};
}

} // namespace

std::unordered_map<std::string, RegSet> buildLiveOut(const NasmCfg &cfg) {
  // build the UEVar and VarKill
  std::unordered_map<std::string, RegSet> ueVar, varKill;
  for (auto &kv : cfg.getNodes())
    std::tie(ueVar[kv.first], varKill[kv.first]) =
        buildUEVarAndVarKill(kv.second);

  std::unordered_map<std::string, RegSet> liveOut;

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &kv : cfg.getNodes()) {
      auto &bbLiveOut = liveOut[kv.first];
      auto oldSz = bbLiveOut.size();

      // recompute bbLiveOut
      // LiveOut(n) =
      //   Union_{succ m} (UEVar(m) union (LiveOut(m) intersect not VarKill(m)))
      for (auto &succ : kv.second.getSuccs()) {
        auto s = ueVar.at(succ);
        auto varKillOfSucc = varKill.at(succ);
        for (auto &v : liveOut[succ]) {
          if (varKillOfSucc.find(v) == varKillOfSucc.end()) {
            s.emplace(v);
          }
        }

        for (auto &v : s)
          bbLiveOut.emplace(v);
      }

      auto newSz = bbLiveOut.size();
      if (oldSz != newSz)
        changed = true;
    }
  }

  return liveOut;
}
} // namespace mocker