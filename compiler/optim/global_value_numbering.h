// Hash-Based Global Value Numbering
// Value Numbering, PRESTON BRIGGS, KEITH D. COOPER, L. TAYLOR SIMPSON, 1997

#ifndef MOCKER_GLOBAL_VALUE_NUMBERING_H
#define MOCKER_GLOBAL_VALUE_NUMBERING_H

#include <list>
#include <unordered_map>
#include <vector>
#include <cassert>

#include "analysis/dominance.h"
#include "opt_pass.h"
#include "set_operation.h"
#include "ir/helper.h"

namespace mocker {
namespace detail {

class ValueNumberTable {
public:
  ValueNumberTable() = default;

  std::shared_ptr<ir::Addr> get(const std::shared_ptr<ir::Addr> &addr);

  void set(const std::shared_ptr<ir::Reg> &reg,
           const std::shared_ptr<ir::Addr> &valueNumber);

  bool has(const std::shared_ptr<ir::Addr> & addr) const {
    auto reg = ir::dyc<ir::Reg>(addr);
    if (!reg)
      return true;
    return isIn(vn, reg);
  }

private:
  ir::RegMap<std::shared_ptr<ir::Addr>> vn;
};

// Map each expression, namely, the right hand side of the instruction, to a
// unique Key.
class InstHash {
public:
  using Key = std::string;
  InstHash() = default;

  Key operator()(const std::shared_ptr<ir::IRInst> &inst);

};

} // namespace detail

class GlobalValueNumbering : public FuncPass {
public:
  explicit GlobalValueNumbering(ir::FunctionModule &func);

  bool operator()() override;

private:
  using ExprRegMap = std::unordered_map<typename detail::InstHash::Key,
                                        std::shared_ptr<ir::Reg>>;

  void doValueNumbering(std::size_t bbLabel,
                        detail::ValueNumberTable valueNumber,
                        detail::InstHash instHash, ExprRegMap exprReg);

  bool canProcessPhi(const std::shared_ptr<ir::Phi> & phi, const detail::ValueNumberTable & vn) const;

private:
  DominatorTree dominatorTree;
  std::size_t cnt = 0;
};

} // namespace mocker

#endif // MOCKER_GLOBAL_VALUE_NUMBERING_H
