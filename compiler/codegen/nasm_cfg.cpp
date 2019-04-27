#include "nasm_cfg.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <vector>

namespace mocker {
void NasmCfg::addNode(LineIter beg, LineIter end) {
  auto label = beg->label;
  assert(!label.empty());
  Node node{beg, end};
  auto success = nodes.emplace(std::make_pair(label, std::move(node))).second;
  assert(success);
}

const std::unordered_map<std::string, NasmCfg::Node> &
NasmCfg::getNodes() const {
  return nodes;
}

void NasmCfg::buildGraph() {
  for (auto &kv : nodes) {
    auto &node = kv.second;
    auto u = kv.first;

    std::vector<std::shared_ptr<nasm::Inst>> terminators;
    for (auto riter = std::make_reverse_iterator(node.getLines().second);;
         ++riter) {
      auto inst = riter->inst;
      if (!inst) {
        break;
      }
      if (!nasm::dyc<nasm::Jmp>(inst) && !nasm::dyc<nasm::CJump>(inst) &&
          !nasm::dyc<nasm::Ret>(inst))
        break;
      terminators.emplace_back(inst);
    }
    std::reverse(terminators.begin(), terminators.end());

    if (terminators.empty()) {
      auto fallThrough = node.getLines().second->label;
      assert(!fallThrough.empty());
      node.succs.emplace_back(fallThrough);
      continue;
    }

    for (auto &terminator : terminators) {
      if (auto j = nasm::dyc<nasm::Jmp>(terminator)) {
        auto v = j->getLabel()->getName();
        node.succs.emplace_back(v);
        break;
      }
      if (auto r = nasm::dyc<nasm::Ret>(terminator)) {
        break;
      }
      if (auto cj = nasm::dyc<nasm::CJump>(terminator)) {
        auto v = cj->getLabel()->getName();
        node.succs.emplace_back(v);
        continue;
      }
    }
    if (auto p = nasm::dyc<nasm::CJump>(terminators.back())) {
      auto fallThrough = node.getLines().second->label;
      assert(!fallThrough.empty());
      node.succs.emplace_back(fallThrough);
    }
  }
}

} // namespace mocker

namespace mocker {
namespace {

std::vector<LineIter> splitFunctionIntoBlocks(LineIter funcBeg,
                                              LineIter funcEnd) {
  assert(!funcBeg->inst);
  std::vector<LineIter> res;
  for (auto iter = funcBeg; iter != funcEnd; ++iter) {
    if (iter->label.empty())
      continue;
    res.emplace_back(iter);
  }
  return res;
}

} // namespace

NasmCfg buildNasmCfg(LineIter funcBeg, LineIter funcEnd) {
  NasmCfg cfg;

  auto blockIters = splitFunctionIntoBlocks(funcBeg, funcEnd);
  blockIters.emplace_back(funcEnd);
  // add the nodes
  for (std::size_t i = 0; i < blockIters.size() - 1; ++i)
    cfg.addNode(blockIters[i], blockIters[i + 1]);

  cfg.buildGraph();

  return cfg;
}

} // namespace mocker
