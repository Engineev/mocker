#include "module.h"

#include <cassert>
#include <string>
#include <vector>

namespace mocker {
namespace ir {

BasicBlock::BasicBlock(const std::reference_wrapper<FunctionModule> &func,
                       size_t labelID)
    : func(func), labelID(labelID) {}

void BasicBlock::appendFront(std::shared_ptr<IRInst> inst) {
  insts.emplace_front(std::move(inst));
}

void BasicBlock::appendInst(std::shared_ptr<IRInst> inst) {
  insts.emplace_back(std::move(inst));
}

bool BasicBlock::isCompleted() const {
  // TODO: other terminators
  if (insts.empty())
    return false;
  return (bool)std::dynamic_pointer_cast<Ret>(insts.back());
}

FunctionModule::FunctionModule(std::string identifier,
                               std::vector<std::string> args, bool isExternal)
    : identifier(std::move(identifier)), args(std::move(args)),
      isExternal(isExternal) {}

BBLIter FunctionModule::pushBackBB() {
  bbs.emplace_back(*this, bbsSz++);
  return --bbs.end();
}

BBLIter FunctionModule::insertBBAfter(BBLIter iter) {
  assert(iter != bbs.end());
  ++iter;
  return bbs.emplace(iter, *this, bbsSz++);
}

} // namespace ir
} // namespace mocker
