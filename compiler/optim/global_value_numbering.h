// Hash-Based Global Value Numbering
// Value Numbering, PRESTON BRIGGS, KEITH D. COOPER, L. TAYLOR SIMPSON, 1997

#ifndef MOCKER_GLOBAL_VALUE_NUMBERING_H
#define MOCKER_GLOBAL_VALUE_NUMBERING_H

#include <list>
#include <unordered_map>
#include <vector>

#include "analysis/dominance.h"
#include "opt_pass.h"

#include "ir/helper.h"
#include <cassert>

namespace mocker {
namespace detail {

class ValueNumberTable {
public:
  ValueNumberTable() = default;

  std::shared_ptr<ir::Addr> get(const std::shared_ptr<ir::Addr> &addr);

  void set(const std::shared_ptr<ir::Reg> &reg,
           const std::shared_ptr<ir::Addr> &valueNumber);

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

private:
  class History {
  public:
    History() = default;
    explicit History(std::int64_t lit) : literal(lit) {}
    History(const History &) = default;
    History &operator=(const History &) = default;
    ~History() = default;

    const bool operator==(const History &rhs) const {
      return !(*this < rhs) && !(rhs < *this);
    }

    const bool operator!=(const History &rhs) const { return !(*this == rhs); }

    const bool operator<(const History &rhs) const {
      return toString() < rhs.toString();
    }

    const bool operator>(const History &rhs) const {
      return !(*this < rhs) && !(*this == rhs);
    }

  public:
    std::string toString() const {
      std::string res = "(";
      res += std::to_string(literal) + ";";
      for (auto &reg : add)
        res += reg->getIdentifier() + ",";
      if (add.empty())
        res += ";";
      else
        res.back() = ';';
      for (auto &reg : sub)
        res += reg->getIdentifier() + ",";
      if (sub.empty())
        res += ")";
      else
        res.back() = ')';
      return res;
    }

    std::int64_t getLiteral() const { return literal; }

    void clear() {
      literal = 0;
      add.clear();
      sub.clear();
    }

    void addLit(std::int64_t lit) { literal += lit; }

    void update(const std::shared_ptr<ir::Addr> &addr, bool isAddition = true) {
      if (auto p = ir::dyc<ir::IntLiteral>(addr)) {
        std::int64_t lit = p->getVal();
        if (!isAddition)
          lit = -lit;
        literal += lit;
        return;
      }
      update(ir::dyc<ir::Reg>(addr), isAddition);
    }

    void neg() {
      std::swap(add, sub);
      literal = -literal;
    }

    bool isLiteral() const { return add.empty() && sub.empty(); }

  private:
    void update(const std::shared_ptr<ir::Reg> &reg, bool isAddition) {
      auto removeIfExists = [](std::vector<std::shared_ptr<ir::Reg>> &l,
                               const std::shared_ptr<ir::Reg> &reg) {
        for (auto iter = l.begin(), end = l.end(); iter != end; ++iter) {
          if ((*iter)->getIdentifier() == reg->getIdentifier()) {
            l.erase(iter);
            return true;
          }
        }
        return false;
      };
      auto insert = [](std::vector<std::shared_ptr<ir::Reg>> &l,
                       const std::shared_ptr<ir::Reg> &reg) {
        for (auto iter = l.begin(), end = l.end(); iter != end; ++iter) {
          if ((*iter)->getIdentifier() >= reg->getIdentifier()) {
            l.insert(iter, reg);
            return;
          }
        }
        l.emplace_back(reg);
      };

      if (isAddition) {
        if (!removeIfExists(sub, reg))
          insert(add, reg);
        return;
      }
      if (!removeIfExists(add, reg))
        insert(sub, reg);
    }

  private:
    std::int64_t literal = 0;
    std::vector<std::shared_ptr<ir::Reg>> add, sub;
  };

  History getOrSet(const std::shared_ptr<ir::Addr> &addr) {
    auto iter = history.find(addr);
    if (iter != history.end())
      return iter->second;
    auto &entry = history[addr];
    entry.update(addr);
    return entry;
  }

  Key hashArithUnary(const std::shared_ptr<ir::ArithUnaryInst> &inst);

  Key hashArithBinary(const std::shared_ptr<ir::ArithBinaryInst> &inst);

  Key hashAddSub(const std::shared_ptr<ir::ArithBinaryInst> &inst);

  Key hashRelation(const std::shared_ptr<ir::RelationInst> &inst);

private:
  struct AddrHash {
    std::size_t operator()(const std::shared_ptr<ir::Addr> &addr) const {
      if (auto reg = ir::dyc<ir::Reg>(addr))
        return std::hash<std::string>{}(reg->getIdentifier());
      auto lit = ir::dyc<ir::IntLiteral>(addr);
      assert(lit);
      return std::hash<std::string>{}(std::to_string(lit->getVal()));
    }
  };

  struct AddrEqual {
    bool operator()(const std::shared_ptr<ir::Addr> &lhs,
                    const std::shared_ptr<ir::Addr> &rhs) const {
      auto lhsLit = ir::dyc<ir::IntLiteral>(lhs),
           rhsLit = ir::dyc<ir::IntLiteral>(rhs);
      auto lhsReg = ir::dyc<ir::Reg>(lhs), rhsReg = ir::dyc<ir::Reg>(rhs);
      if (lhsLit && rhsLit)
        return lhsLit->getVal() == rhsLit->getVal();
      if (lhsReg && rhsReg)
        return lhsReg->getIdentifier() == rhsReg->getIdentifier();
      return false;
    }
  };

  std::unordered_map<std::shared_ptr<ir::Addr>, History, AddrHash, AddrEqual>
      history;
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

private:
  DominatorTree dominatorTree;
  std::size_t cnt = 0;
};

} // namespace mocker

#endif // MOCKER_GLOBAL_VALUE_NUMBERING_H
