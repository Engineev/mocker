#ifndef MOCKER_CONSTANT_PROPAGATION_H
#define MOCKER_CONSTANT_PROPAGATION_H

#include <cassert>
#include <queue>
#include <unordered_map>

#include "opt_pass.h"

namespace mocker {

// An implementation of the algorithm described in Sec. 9.3.6, EaC, 2nd.
class SparseSimpleConstantPropagation : public FuncPass {
public:
  explicit SparseSimpleConstantPropagation(ir::FunctionModule &func);

  void operator()() override;

private:
  struct Value {
    enum Type { Top, Constant, Bottom };

    Value() : type(Type::Top), val(0) {}
    explicit Value(Type type) : type(type), val(0) { assert(type != Constant); }
    explicit Value(int64_t val) : type(Constant), val(val) {}

    bool operator==(const Value &rhs) const {
      return type == rhs.type && val == rhs.val;
    }

    bool operator!=(const Value &rhs) const { return !(*this == rhs); }

    Type type;
    std::int64_t val;
  };

  template <class T, class V> decltype(auto) dyc(V &&v) const {
    return std::dynamic_pointer_cast<T>(v);
  }

  void buildInstDefineAndInstsUse();

  void initialize();

  void propagate();

  void rewrite();

private:
  Value computeValue(const std::string &destName);

  Value getValue(const std::shared_ptr<ir::Addr> &addr);

private:
  std::unordered_map<std::string, std::shared_ptr<ir::IRInst>> instDefine;
  std::unordered_map<std::string, std::vector<std::shared_ptr<ir::IRInst>>>
      instsUse;
  std::unordered_map<std::string, Value> values; // values of an SSA name
  std::queue<std::string> workList;

  std::size_t modificationCnt = 0;
};

} // namespace mocker

#endif // MOCKER_CONSTANT_PROPAGATION_H
