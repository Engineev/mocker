#include "register_allocation.h"

#include <random>

#include "nasm/helper.h"

#include "nasm/printer.h"
#include <fstream>
#include <iostream>

namespace mocker {
namespace detail {
namespace {

const std::vector<std::shared_ptr<nasm::Register>> Colors = {
    nasm::rax(), nasm::rcx(), nasm::rdx(), nasm::rbx(), nasm::rsi(),
    nasm::rdi(), nasm::r8(),  nasm::r9(),  nasm::r10(), nasm::r11(),
    nasm::r12(), nasm::r13(), nasm::r14(), nasm::r15()};
constexpr std::size_t K = 14;

} // namespace

} // namespace detail
} // namespace mocker

namespace mocker {
namespace detail {

std::vector<Node> getNonPrecoloredNodes(LineIter funcBeg, LineIter funcEnd) {
  RegSet s;
  for (auto iter = funcBeg; iter != funcEnd; ++iter) {
    auto inst = iter->inst;
    if (!inst)
      continue;
    unionSet(s, nasm::getInvolvedRegs(inst));
  }
  std::vector<Node> res;
  for (auto &n : s) {
    if (!isPrecolored(n))
      res.emplace_back(n);
  }
  return res;
}

} // namespace detail
} // namespace mocker

// InterferenceGraph
namespace mocker {
namespace detail {

void InterferenceGraph::clearAndInit(const std::vector<Node> &nonPrecolored) {
  edges.clear();
  originalAdjList.clear();
  curDegree.clear();
  simplifiable.clear();
  highDegree.clear();
  freeze.clear();
  coalesced.clear();
  removed.clear();
  alias.clear();
  selectStack.clear();

  for (auto &n : nonPrecolored) {
    originalAdjList[n] = {};
    curDegree[n] = 0;
    alias[n] = n;
  }
  std::vector<Node> machineRegs = {
      nasm::rax(), nasm::rcx(), nasm::rdx(), nasm::rbx(),
      nasm::rsp(), nasm::rbp(), nasm::rsi(), nasm::rdi(),
      nasm::r8(),  nasm::r9(),  nasm::r10(), nasm::r11(),
      nasm::r12(), nasm::r13(), nasm::r14(), nasm::r15()};
  for (auto &n : machineRegs) {
    curDegree[n] = nonPrecolored.size() + 30;
    alias[n] = n;
  }
}

void InterferenceGraph::build(
    LineIter funcBeg, LineIter funcEnd,
    const std::function<void(const MovInst &)> &pushWorklist,
    const std::function<void(const MovInst &)> &updateAssociatedMove) {
  auto cfg = buildNasmCfg(funcBeg, funcEnd);
  auto liveOut = buildLiveOut(cfg);

  auto dycMov = [](const std::shared_ptr<nasm::Inst> &inst)
      -> std::shared_ptr<nasm::Mov> {
    auto mov = nasm::dyc<nasm::Mov>(inst);
    if (!mov || !nasm::dyc<nasm::Register>(mov->getOperand()) ||
        !nasm::dyc<nasm::Register>(mov->getDest()))
      return nullptr;
    return mov;
  };

  for (auto &kv : cfg.getNodes()) {
    auto &bbIdent = kv.first;
    auto &bb = kv.second;
    auto live = liveOut.at(bbIdent);
    auto rbeg = std::make_reverse_iterator(bb.getLines().second);
    auto rend = std::make_reverse_iterator(bb.getLines().first);
    for (auto riter = rbeg; riter != rend; ++riter) {
      auto inst = riter->inst;
      if (!inst)
        continue;
      auto defs = nasm::getDefinedRegs(inst);
      auto uses = nasm::getUsedRegs(inst);

      if (auto mov = dycMov(inst)) {
        subSet(live, uses);
        updateAssociatedMove(mov);
        pushWorklist(mov);
      }

      unionSet(live, defs);
      for (auto &d : defs) {
        for (auto &l : live)
          addEdge(l, d);
      }
      subSet(live, defs);
      unionSet(live, uses);
    }
  }
}

bool InterferenceGraph::isOfHighDegree(const Node &n) const {
  return curDegree.at(n) >= K;
}

void InterferenceGraph::initWorklists(const std::vector<Node> &initial) {
  for (auto &node : initial) {
    classifyNode(node);
  }
}

std::pair<std::vector<Node>, RegMap<Node>> InterferenceGraph::assignColors() {
  std::vector<Node> toBeSpilled;
  RegMap<Node> coloring;

  auto removeIfExists = [](RegSet &s, const Node &n) {
    auto iter = s.find(n);
    if (iter != s.end())
      s.erase(iter);
  };

  while (!selectStack.empty()) {
    auto n = selectStack.back();
    selectStack.pop_back();

    RegSet okColors(Colors.begin(), Colors.end());
    for (auto neighbor : originalAdjList.at(n)) {
      neighbor = getAlias(neighbor);
      if (isPrecolored(neighbor))
        removeIfExists(okColors, neighbor);
      else if (isIn(coloring, neighbor))
        removeIfExists(okColors, coloring.at(neighbor));
    }

    if (okColors.empty()) {
      toBeSpilled.emplace_back(n);
      continue;
    }
    coloring[n] = *okColors.begin();
  }

  for (auto &n : coalesced) {
    auto tmp = getAlias(n);
    coloring[n] = isPrecolored(tmp) ? tmp : coloring.at(getAlias(n));
  }

  return {toBeSpilled, coloring};
}

std::vector<Node> InterferenceGraph::getCurAdjList(const Node &node) const {
  std::vector<Node> res;
  for (auto &n : originalAdjList.at(node)) {
    if (!isIn(removed, n) && !isIn(coalesced, n))
      res.emplace_back(n);
  }
  return res;
}

bool InterferenceGraph::simplify() {
  if (simplifiable.empty())
    return false;

  auto n = *simplifiable.begin();
  assert(!isMoveRelated(n));
  //  std::cerr << "simplify: " << n->getIdentifier() << std::endl;
  simplifiable.erase(simplifiable.begin());
  removed.emplace(n);
  selectStack.push_back(n);

  for (auto &m : getCurAdjList(n))
    decrementDegree(m);

  return true;
}

void InterferenceGraph::decrementDegree(const Node &m) {
  auto d = curDegree[m]--;
  if (d != K)
    return;

  enableMoves(m);
  for (auto &neighbor : getCurAdjList(m))
    enableMoves(neighbor);

  highDegree.erase(highDegree.find(m));
  if (isMoveRelated(m))
    freeze.emplace(m);
  else
    simplifiable.emplace(m);
}

void InterferenceGraph::addEdge(Node u, Node v) {
  if (u->getIdentifier() == v->getIdentifier())
    return;
  if (u->getIdentifier() < v->getIdentifier())
    std::swap(u, v);

  auto found = !edges.emplace(std::make_pair(u, v)).second;
  if (found)
    return;
  if (!isPrecolored(u)) {
    originalAdjList.at(u).emplace(v);
    curDegree.at(u)++;
  }
  if (!isPrecolored(v)) {
    originalAdjList.at(v).emplace(u);
    curDegree.at(v)++;
  }
}

Node InterferenceGraph::getAlias(const Node &n) const {
  if (isIn(coalesced, n))
    return getAlias(alias.at(n));
  return n;
}

void InterferenceGraph::classifyNode(const Node &n) {
  assert(!isPrecolored(n));
  assert(coalesced.find(n) == coalesced.end());
  assert(removed.find(n) == removed.end());

  auto removeIfExists = [&n](RegSet &s) {
    auto iter = s.find(n);
    if (iter != s.end())
      s.erase(iter);
  };

  removeIfExists(simplifiable);
  removeIfExists(highDegree);
  removeIfExists(freeze);
  if (isOfHighDegree(n)) {
    highDegree.emplace(n);
    return;
  }
  if (isMoveRelated(n)) {
    freeze.emplace(n);
    return;
  }
  simplifiable.emplace(n);
}

bool InterferenceGraph::conservative(const Node &x, const Node &y) const {
  auto neighborsOfX = getCurAdjList(x);
  RegSet neighbors(neighborsOfX.begin(), neighborsOfX.end());
  unionSet(neighbors, getCurAdjList(y));
  std::size_t k = 0;
  for (auto &m : neighbors) {
    if (curDegree.at(m) >= K)
      ++k;
  }
  return k < K;
}

bool InterferenceGraph::isCoalesceAbleToAMachineReg(const Node &v,
                                                    const Node &r) {
  // Do not use getCurAdjList here.
  // This condition usually fails very soon, whence we do need to build the
  // entire adjacent list.

  for (auto &t : originalAdjList.at(v)) {
    // Note that since most nodes removed are of degree < K, we may omit the
    // isIn(removed, t) here to speed up the process.
    if (isIn(coalesced, t) || isIn(removed, t))
      continue;
    if (curDegree.at(t) < K)
      continue;
    if (isPrecolored(t))
      continue;
    if (isAdjacent(t, r))
      continue;
    return false;
  }
  return true;
}

void InterferenceGraph::combine(const Node &v, const Node &u) {
  if (isIn(freeze, v))
    freeze.erase(freeze.find(v));
  else
    highDegree.erase(highDegree.find(v));
  coalesced.emplace(v);
  alias.at(v) = u;
  // The associated moves are handle externally

  for (auto &t : getCurAdjList(v)) {
    // Note that we do not remove node v explicitly. Hence, we have to
    // decrement the degree of t manually.
    addEdge(t, u);
    curDegree.at(t)--;

    // Do not use decrementDegree here since it may trigger some unnecessary
    // task if the original degree of t is K
    //    if (!isPrecolored(t))
    //      classifyNode(t);
    //    decrementDegree(t);
  }
  if (!isPrecolored(u))
    classifyNode(u);
}

bool InterferenceGraph::freezeNode() {
  if (freeze.empty())
    return false;
  auto u = *freeze.begin();
  //  std::cerr << "freeze: " << u->getIdentifier() << ": ";
  freeze.erase(freeze.begin());
  simplifiable.emplace(u);

  auto movs = freezeMoves(u);
  for (auto &m : movs) {
    auto x = nasm::dyc<nasm::Register>(m->getDest());
    auto y = nasm::dyc<nasm::Register>(m->getOperand());
    auto v = getAlias(y);
    if (getAlias(y)->getIdentifier() == getAlias(u)->getIdentifier())
      v = getAlias(x);
    if (!isPrecolored(v))
      classifyNode(v);
  }
  //  std::cerr << std::endl;
  return true;
}

bool InterferenceGraph::spill() {
  if (highDegree.empty())
    return false;

  auto resIter = highDegree.begin();
  std::size_t maxDeg = 0;
  for (auto iter = highDegree.begin(), end = highDegree.end(); iter != end;
       ++iter) {
    auto n = *iter;
    if (curDegree.at(n) > maxDeg) {
      resIter = iter;
      maxDeg = curDegree.at(n);
    }
  }

  // resIter = highDegree.begin();
  auto m = *resIter;
  highDegree.erase(resIter);
  simplifiable.emplace(m);
  freezeMoves(m);
  return true;
}

bool InterferenceGraph::isAdjacent(Node u, Node v) const {
  if (u->getIdentifier() < v->getIdentifier())
    std::swap(u, v);
  return edges.find(std::make_pair(u, v)) != edges.end();
}

} // namespace detail
} // namespace mocker

// MoveInfo
namespace mocker {
namespace detail {

void MoveInfo::clearAndInit(const std::vector<Node> &nonPrecolored) {
  active.clear();
  constrained.clear();
  frozen.clear();
  worklist.clear();
  associatedMoves.clear();
  for (auto &n : nonPrecolored) {
    associatedMoves[n] = {};
  }
  for (auto &n : Colors) {
    associatedMoves[n] = {};
  }
}

std::vector<MovInst> MoveInfo::getCurAssociatedMoves(const Node &n) {
  std::vector<MovInst> res;
  for (auto &mov : associatedMoves.at(n)) {
    if (isIn(worklist, mov) || isIn(active, mov))
      res.emplace_back(mov);
  }
  return res;
}

void MoveInfo::updateAssociatedMoves(const MovInst &mov) {
  auto dest = nasm::dyc<nasm::Register>(mov->getDest());
  assert(dest);
  associatedMoves[dest].emplace(mov);
  auto operand = nasm::dyc<nasm::Register>(mov->getOperand());
  assert(operand);
  associatedMoves[operand].emplace(mov);
}

void MoveInfo::enableMove(const MovInst &mov) {
  auto iter = active.find(mov);
  if (iter == active.end())
    return;
  active.erase(iter);
  worklist.emplace(mov);
}

void MoveInfo::mergeAssociatedMoves(const Node &u, const Node &v) {
  unionSet(associatedMoves.at(u), associatedMoves.at(v));
}

MovInst MoveInfo::popWorklist() {
  auto res = *worklist.begin();
  worklist.erase(worklist.begin());
  return res;
}

std::vector<MovInst> MoveInfo::freezeMoves(const Node &n) {
  auto movs = getCurAssociatedMoves(n);
  std::vector<MovInst> res(movs.begin(), movs.end());

  for (auto &mov : movs) {
    frozen.emplace(mov);
    if (isIn(active, mov))
      active.erase(active.find(mov));
    else
      worklist.erase(worklist.find(mov));
  }

  return res;
}

bool MoveInfo::isMoveRelated(const Node &n) const {
  auto &movs = associatedMoves.at(n);
  for (auto &mov : movs) {
    if (isIn(worklist, mov))
      return true;
    if (isIn(active, mov))
      return true;
  }
  return false;
}

} // namespace detail
} // namespace mocker

// RegisterAllocator
namespace mocker {
namespace detail {

void RegisterAllocator::allocate() {
  auto enableMoves = [this](const Node &n) {
    auto movs = moveInfo.getCurAssociatedMoves(n);
    for (auto &mov : movs)
      moveInfo.enableMove(mov);
  };
  auto isMoveRelated = [this](const Node &n) {
    return moveInfo.isMoveRelated(n);
  };
  auto freezeMoves = [this](const Node &n) { return moveInfo.freezeMoves(n); };

  while (true) {
    auto nonPrecolored = getNonPrecoloredNodes(funcBeg, funcEnd);
    interG.clearAndInit(nonPrecolored);
    interG.bindFunctions(enableMoves, isMoveRelated, freezeMoves);
    moveInfo.clearAndInit(nonPrecolored);

    interG.build(
        funcBeg, funcEnd,
        [this](const MovInst &mov) { moveInfo.pushWorklist(mov); },
        [this](const MovInst &mov) { moveInfo.updateAssociatedMoves(mov); });
    interG.initWorklists(nonPrecolored);

    while (true) {
      if (interG.simplify())
        continue;
      if (coalesce())
        continue;
      if (interG.freezeNode())
        continue;
      if (interG.spill())
        continue;
      break;
    }

    std::vector<Node> toBeSpilled;
    RegMap<Node> coloring;
    std::tie(toBeSpilled, coloring) = interG.assignColors();
    if (toBeSpilled.empty()) {
      color(coloring);
      break;
    }
    spilledLastTime = RegSet(toBeSpilled.begin(), toBeSpilled.end());
    std::cerr << "\nto be spilled: ";
    for (auto &reg : toBeSpilled)
      std::cerr << reg->getIdentifier() << ", ";
    std::cerr << std::endl;
    rewriteProgram(toBeSpilled);
  }
}

bool RegisterAllocator::coalesce() {
  if (moveInfo.isWorklistEmpty())
    return false;

  auto mov = moveInfo.popWorklist();
  auto x = nasm::dyc<nasm::Register>(mov->getDest());
  auto y = nasm::dyc<nasm::Register>(mov->getOperand());
  x = interG.getAlias(x);
  y = interG.getAlias(y);

  // A clearly unnecessary move
  if (x->getIdentifier() == y->getIdentifier()) {
    moveInfo.pushCoalescedMove(mov);
    if (!isPrecolored(x))
      interG.classifyNode(x);
    return true;
  }

  // A constrained move
  if ((isPrecolored(x) && isPrecolored(y)) || interG.isAdjacent(x, y)) {
    moveInfo.pushConstrainedMove(mov);
    if (!isPrecolored(x))
      interG.classifyNode(x);
    if (!isPrecolored(y))
      interG.classifyNode(y);
    return true;
  }

  // Make sure that the only node can be colored is x, so that after the
  // coalescing, the left node is always x
  if (isPrecolored(y))
    std::swap(x, y);

  // If neither x nor y is colored, use Brigg's conservative criterion
  // If x is colored, then use the heuristic
  if ((!isPrecolored(x) && interG.conservative(x, y)) ||
      (isPrecolored(x) && interG.isCoalesceAbleToAMachineReg(y, x))) {
    moveInfo.pushCoalescedMove(mov);
    interG.combine(y, x);                // combine y into x
    moveInfo.mergeAssociatedMoves(x, y); // merge NodeMove(y) into NodeMove(x)
    if (!isPrecolored(x))
      interG.classifyNode(x);
    return true;
  }

  moveInfo.pushActiveMove(mov);
  return true;
}

void RegisterAllocator::color(const RegMap<Node> &coloring) {
  std::list<nasm::Line> insts;
  for (auto iter = funcBeg; iter != funcEnd; ++iter) {
    auto &inst = iter->inst;
    if (!inst) {
      insts.emplace_back(*iter);
      continue;
    }
    auto newInst = nasm::replaceRegs(inst, coloring);
    if (auto p = nasm::dyc<nasm::Mov>(newInst)) {
      auto dest = nasm::dyc<nasm::Register>(p->getDest());
      auto src = nasm::dyc<nasm::Register>(p->getOperand());
      if (dest && src && dest->getIdentifier() == src->getIdentifier()) {
        insts.emplace_back(iter->label);
        continue;
      }
    }
    insts.emplace_back(iter->label, newInst);
  }

  funcBeg = funcEnd = section.erase(funcBeg, funcEnd);
  --funcBeg;
  for (auto &inst : insts) {
    section.appendLine(funcEnd, std::move(inst));
  }
  ++funcBeg;
}

void RegisterAllocator::rewriteProgram(const std::vector<Node> &toBeSpilled) {
  // find sub rsp
  LineIter iter;
  for (iter = funcBeg; iter != funcEnd; ++iter) {
    auto inst = nasm::dyc<nasm::BinaryInst>(iter->inst);
    if (!inst || inst->getType() != nasm::BinaryInst::Sub)
      continue;
    auto reg = nasm::dyc<nasm::Register>(inst->getLhs());
    if (reg->getIdentifier() == nasm::rsp()->getIdentifier())
      break;
  }

  std::size_t baseOffset = 0;
  RegMap<std::shared_ptr<nasm::MemoryAddr>> offsets;

  // An sub-rsp instruction exists
  if (iter != funcEnd) {
    auto inst = nasm::dyc<nasm::BinaryInst>(iter->inst);
    baseOffset = nasm::dyc<nasm::NumericConstant>(inst->getRhs())->getVal();
    int64_t alignSz = baseOffset + 8 * toBeSpilled.size() +
                      (16 - (8 * toBeSpilled.size()) % 16);
    auto label = iter->label;
    auto pos = section.erase(iter);
    auto newSubRsp = std::make_shared<nasm::BinaryInst>(
        nasm::BinaryInst::Sub, nasm::rsp(),
        std::make_shared<nasm::NumericConstant>(alignSz));
    section.appendLine(pos, nasm::Line(label, newSubRsp));

  } else {
    for (iter = funcBeg; iter != funcEnd; ++iter) {
      if (iter->inst)
        break;
    }
    // The first two instructions of a function are always
    // push  %rbp
    // mov   %rbp, %rsp
    ++iter;
    ++iter;
    int64_t alignSz = baseOffset + 8 * toBeSpilled.size() +
                      (16 - (8 * toBeSpilled.size()) % 16);
    auto newSubRsp = std::make_shared<nasm::BinaryInst>(
        nasm::BinaryInst::Sub, nasm::rsp(),
        std::make_shared<nasm::NumericConstant>(alignSz));
    section.appendLine(iter, nasm::Line("", newSubRsp));
  }

  for (std::size_t i = 0, sz = toBeSpilled.size(); i < sz; ++i) {
    offsets[toBeSpilled[i]] = std::make_shared<nasm::MemoryAddr>(
        nasm::rbp(), -((int)baseOffset + 8 * (i + 1)));
  }

  std::size_t vRegCnt = 0;
  auto newVReg = [&vRegCnt](const std::string &hint) {
    return std::make_shared<nasm::Register>(hint + "_" +
                                            std::to_string(vRegCnt++));
  };
  std::list<nasm::Line> lines;
  RegSet s(toBeSpilled.begin(), toBeSpilled.end());
  for (iter = funcBeg; iter != funcEnd; ++iter) {
    auto inst = iter->inst;
    if (!inst) {
      lines.emplace_back(*iter);
      continue;
    }
    RegMap<Node> mp;
    for (auto &reg : nasm::getInvolvedRegs(inst)) {
      if (!isIn(s, reg))
        continue;
      mp[reg] = newVReg(reg->getIdentifier());
    }
    for (auto &reg : nasm::getUsedRegs(inst)) {
      if (!isIn(s, reg))
        continue;
      auto load = std::make_shared<nasm::Mov>(mp.at(reg), offsets.at(reg));
      lines.emplace_back(load);
    }
    auto newInst = nasm::replaceRegs(inst, mp);
    lines.emplace_back(iter->label, newInst);
    for (auto &reg : nasm::getDefinedRegs(inst)) {
      if (!isIn(s, reg))
        continue;
      auto storeBack = std::make_shared<nasm::Mov>(offsets.at(reg), mp.at(reg));
      lines.emplace_back(storeBack);
    }
  }

  funcBeg = funcEnd = section.erase(funcBeg, funcEnd);
  bool flag = false;
  for (auto &line : lines) {
    section.appendLine(funcEnd, std::move(line));
    if (!flag) {
      flag = true;
      --funcBeg;
    }
  }
}

} // namespace detail
} // namespace mocker

namespace mocker {

nasm::Module allocateRegisters(const nasm::Module &module) {
  auto res = module;
  auto &text = res.getSection(".text");
  std::vector<LineIter> funcBegs;
  for (auto iter = text.getLines().begin(); iter != text.getLines().end();
       ++iter) {
    if (!iter->inst && iter->label.at(0) != '.')
      funcBegs.emplace_back(iter);
  }

  funcBegs.emplace_back(text.getLines().end());
  for (auto iter = funcBegs.begin(), ed = std::prev(funcBegs.end()); iter != ed;
       ++iter) {
    detail::RegisterAllocator(text, *iter, *(iter + 1)).allocate();
  }
  return res;
}

} // namespace mocker