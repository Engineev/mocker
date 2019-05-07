#ifndef MOCKER_INTERPRETER_H
#define MOCKER_INTERPRETER_H

#include <cassert>
#include <functional>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "ir/helper.h"
#include "ir/ir_inst.h"

namespace mocker {
namespace ir {

class Interpreter {
private:
  using Iter = std::string::const_iterator;
  using Integer = std::int64_t;

public:
  explicit Interpreter(std::string source);

  ~Interpreter();

  std::int64_t run();

private:
  struct FuncModule;
  struct ActivationRecord;

  std::string deleteComments(std::string source) const;

  // "aaaaaaaaaaaaaaaaaaaaa"_
  // ^                      ^
  // input iterator         output iterator
  Iter readStringLiteral(Iter iter, Iter end) const;

  void parse(const std::string &source);

  void parseFuncBody(std::istream &in, FuncModule &func);

  std::shared_ptr<IRInst> parseInst(const std::string &line) const;

  std::shared_ptr<Addr> parseAddr(const std::string &str) const;

private:
  void initExternalFuncs();

  std::int64_t executeFunc(const std::string &funcName,
                           const std::vector<std::int64_t> &args);

  std::int64_t executeFunc(const FuncModule &func,
                           const std::vector<std::int64_t> &args);

  void executePhis(const std::vector<std::shared_ptr<Phi>> &phis);

  std::size_t executeInst(std::size_t idx);

  std::int64_t readVal(const std::shared_ptr<Addr> &reg) const;

  template <class T> void writeReg(const std::shared_ptr<Addr> &reg, T val_) {
    auto val = reinterpret_cast<std::int64_t>(val_);
    printLog(reg, val);
    if (auto p = dycLocalReg(reg)) {
      ars.top().localReg[p->getIdentifier()] = val;
      return;
    }
    if (auto p = dycGlobalReg(reg)) {
      assert(false);
      globalReg[p->getIdentifier()] = val;
      return;
    }
    assert(false);
  }

  // Just for fun...
  void *fastMalloc(std::size_t sz);

  void printLog(const std::shared_ptr<Addr> &addr, std::int64_t val);

  void printLog(std::int64_t addr, std::int64_t val);

  void printLog(const std::string &identfier, std::int64_t val);

private:
  struct FuncModule {
    std::vector<std::shared_ptr<IRInst>> insts;
    std::vector<std::string> args;
    std::unordered_map<std::size_t, std::size_t> label2idx;
    std::size_t firstBB = 0;
  };
  struct ActivationRecord {
    explicit ActivationRecord(const FuncModule &curFunc) : curFunc(curFunc) {}
    ActivationRecord(ActivationRecord &&) = default;

    std::reference_wrapper<const FuncModule> curFunc;
    std::unordered_map<std::string, Integer> localReg;
    std::size_t lastBB = 0, curBB = 0;
    std::int64_t retVal = 0;
  };

  using ExternalFuncType =
      std::function<std::int64_t(const std::vector<std::int64_t> &)>;

  std::vector<GlobalVar> globalVars;
  std::unordered_map<std::string, FuncModule> funcs;
  std::unordered_map<std::string, ExternalFuncType> externalFuncs;
  std::unordered_map<std::string, Integer> globalReg;
  std::stack<ActivationRecord> ars;
  std::vector<void *> malloced;

  // fastMalloc
  static constexpr std::size_t Capacity = 32 * 1024;
  std::size_t curSize = 0;
  std::int8_t fastMem[Capacity];
};

} // namespace ir
} // namespace mocker

#endif // MOCKER_INTERPRETER_H
