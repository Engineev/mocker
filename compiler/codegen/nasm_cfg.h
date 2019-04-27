#ifndef MOCKER_NASM_CFG_H
#define MOCKER_NASM_CFG_H

#include <cassert>
#include <unordered_map>
#include <utility>
#include <vector>

#include "helper.h"
#include "nasm/module.h"

namespace mocker {

class NasmCfg {
public:
  class Node;

  NasmCfg() = default;
  NasmCfg(const NasmCfg &) = default;
  NasmCfg(NasmCfg &&) = default;
  NasmCfg &operator=(const NasmCfg &) = default;
  NasmCfg &operator=(NasmCfg &&) = default;
  ~NasmCfg() = default;

public:
  class Node {
  public:
    std::pair<LineIter, LineIter> getLines() const {
      return {blockBeg, blockEnd};
    }

    const std::vector<std::string> &getSuccs() const { return succs; }

    const std::string &getLabel() const {
      assert(!blockBeg->label.empty());
      return blockBeg->label;
    }

  private:
    friend class NasmCfg;
    Node(LineIter beg, LineIter end) : blockBeg(beg), blockEnd(end) {}

    LineIter blockBeg, blockEnd;
    std::vector<std::string> succs;
  };

  void addNode(LineIter beg, LineIter end);

  void buildGraph();

  const std::unordered_map<std::string, Node> &getNodes() const;

private:
  std::unordered_map<std::string, Node> nodes;
};

NasmCfg buildNasmCfg(LineIter funcBeg, LineIter funcEnd);

} // namespace mocker

#endif // MOCKER_NASM_CFG_H
