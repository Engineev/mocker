#ifndef MOCKER_IR_INST_H
#define MOCKER_IR_INST_H

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mocker {
// Addr
namespace ir {

struct Addr : std::enable_shared_from_this<Addr> {
  virtual ~Addr() = default;
};

struct IntLiteral : Addr {
  explicit IntLiteral(std::int64_t val) : val(val) {}

  std::int64_t val;
};

struct LocalReg : Addr {
  explicit LocalReg(std::string identifier)
      : identifier(std::move(identifier)) {}

  std::string identifier;
};

struct GlobalReg : Addr {
  explicit GlobalReg(std::string identifier)
      : identifier(std::move(identifier)) {}

  std::string identifier;
};

struct Label : Addr {
  explicit Label(size_t id) : id(id) {}

  std::size_t id;
};

} // namespace ir

// IRInst
namespace ir {

using InstID = std::uintptr_t;

class IRInst : public std::enable_shared_from_this<IRInst> {
public:
  virtual ~IRInst() = default;

  InstID getID() const { return (InstID)this; }
};

class Terminator {
public:
  virtual ~Terminator() = default;
};

class Definition {
public:
  explicit Definition(std::shared_ptr<Addr> dest) : dest(std::move(dest)) {}

  virtual ~Definition() = default;

  std::shared_ptr<const Addr> getDest() const { return dest; }

protected:
  std::shared_ptr<Addr> dest;
};

class Deleted : public IRInst {};

class Comment : public IRInst {
public:
  explicit Comment(std::string str) : str(std::move(str)) {}
  explicit Comment(const char *str) : str(str) {}

  const std::string &getContent() const { return str; }

private:
  std::string str;
};

// When printing the IR, a comment of this type will be attached right after the
// next instruction.
class AttachedComment : public IRInst {
public:
  explicit AttachedComment(std::string str) : str(std::move(str)) {}
  explicit AttachedComment(const char *str) : str(str) {}

  const std::string &getContent() const { return str; }

private:
  std::string str;
};

class Assign : public IRInst, public Definition {
public:
  Assign(std::shared_ptr<Addr> dest, std::shared_ptr<Addr> operand)
      : Definition(std::move(dest)), operand(std::move(operand)) {}

  std::shared_ptr<const Addr> getOperand() const { return operand; }

private:
  std::shared_ptr<Addr> operand;
};

class ArithUnaryInst : public IRInst, public Definition {
public:
  enum OpType { Neg, BitNot };

  ArithUnaryInst(std::shared_ptr<Addr> dest, OpType op,
                 std::shared_ptr<Addr> operand)
      : Definition(std::move(dest)), op(op), operand(std::move(operand)) {}

  OpType getOp() const { return op; }

  std::shared_ptr<const Addr> getOperand() const { return operand; }

private:
  OpType op;
  std::shared_ptr<Addr> operand;
};

struct ArithBinaryInst : public IRInst, public Definition {
public:
  // clang-format off
  enum OpType {
    BitOr, BitAnd, Xor,
    Shl, Shr,
    Add, Sub,
    Mul, Div, Mod,
  };
  // clang-format on

  ArithBinaryInst(std::shared_ptr<Addr> dest, OpType op,
                  std::shared_ptr<Addr> lhs, std::shared_ptr<Addr> rhs)
      : Definition(std::move(dest)), op(op), lhs(std::move(lhs)),
        rhs(std::move(rhs)) {}

  OpType getOp() const { return op; }

  std::shared_ptr<const Addr> getLhs() const { return lhs; }

  std::shared_ptr<const Addr> getRhs() const { return rhs; }

private:
  OpType op;
  std::shared_ptr<Addr> lhs, rhs;
};

class RelationInst : public IRInst, public Definition {
public:
  enum OpType { Eq, Ne, Lt, Gt, Le, Ge };

  RelationInst(std::shared_ptr<Addr> dest, OpType op, std::shared_ptr<Addr> lhs,
               std::shared_ptr<Addr> rhs)
      : Definition(std::move(dest)), op(op), lhs(std::move(lhs)),
        rhs(std::move(rhs)) {}

  OpType getOp() const { return op; }

  std::shared_ptr<const Addr> getLhs() const { return lhs; }

  std::shared_ptr<const Addr> getRhs() const { return rhs; }

private:
  OpType op;
  std::shared_ptr<Addr> lhs, rhs;
};

class Store : public IRInst {
public:
  Store(std::shared_ptr<Addr> addr, std::shared_ptr<Addr> val)
      : addr(std::move(addr)), val(std::move(val)) {}

  std::shared_ptr<const Addr> getAddr() const { return addr; }

  std::shared_ptr<const Addr> getVal() const { return val; }

private:
  std::shared_ptr<Addr> addr;
  std::shared_ptr<Addr> val;
};

class Load : public IRInst, public Definition {
public:
  Load(std::shared_ptr<Addr> dest, std::shared_ptr<Addr> addr)
      : Definition(std::move(dest)), addr(std::move(addr)) {}

  std::shared_ptr<const Addr> getAddr() const { return addr; }

private:
  std::shared_ptr<Addr> addr;
};

class Alloca : public IRInst, public Definition {
public:
  Alloca(std::shared_ptr<Addr> dest, size_t size)
      : Definition(std::move(dest)), size(size) {}

  size_t getSize() const { return size; }

private:
  std::size_t size;
};

class Malloc : public IRInst, public Definition {
public:
  Malloc(std::shared_ptr<Addr> dest, std::shared_ptr<Addr> size)
      : Definition(std::move(dest)), size(std::move(size)) {}

  std::shared_ptr<const Addr> getSize() const { return size; }

private:
  std::shared_ptr<Addr> size;
};

class SAlloc : public IRInst, public Definition {
public:
  SAlloc(std::shared_ptr<Addr> dest, size_t size)
      : Definition(std::move(dest)), size(size) {}

  size_t getSize() const { return size; }

private:
  std::size_t size;
};

class StrCpy : public IRInst {
public:
  StrCpy(std::shared_ptr<Addr> addr, std::string data)
      : addr(std::move(addr)), data(std::move(data)) {}

  std::shared_ptr<const Addr> getAddr() const { return addr; }

  const std::string &getData() const { return data; }

private:
  std::shared_ptr<Addr> addr;
  std::string data;
};

class Branch : public IRInst, public Terminator {
public:
  Branch(std::shared_ptr<Addr> condition, std::shared_ptr<Label> then,
         std::shared_ptr<Label> else_)
      : condition(std::move(condition)), then(std::move(then)),
        else_(std::move(else_)) {}

  std::shared_ptr<const Addr> getCondition() const { return condition; }

  std::shared_ptr<const Label> getThen() const { return then; }

  std::shared_ptr<const Label> getElse() const { return else_; }

private:
  std::shared_ptr<Addr> condition;
  std::shared_ptr<Label> then, else_;
};

class Jump : public IRInst, public Terminator {
public:
  explicit Jump(std::shared_ptr<Label> label) : label(std::move(label)) {}

  std::shared_ptr<const Label> getLabel() const { return label; }

private:
  std::shared_ptr<Label> label;
};

class Ret : public IRInst, public Terminator {
public:
  explicit Ret(std::shared_ptr<Addr> val = nullptr) : val(std::move(val)) {}

  std::shared_ptr<const Addr> getVal() const { return val; }

private:
  std::shared_ptr<Addr> val;
};

class Call : public IRInst, public Definition {
public:
  template <class... Args>
  Call(std::shared_ptr<Addr> dest, std::string funcName, Args... args)
      : Definition(std::move(dest)), funcName(std::move(funcName)),
        args({std::move(args)...}) {}

  template <class... Args>
  explicit Call(std::string funcName, Args... args)
      : Definition(nullptr), funcName(std::move(funcName)),
        args({std::move(args)...}) {}

  Call(std::shared_ptr<Addr> dest, std::string funcName,
       std::vector<std::shared_ptr<Addr>> args)
      : Definition(std::move(dest)), funcName(std::move(funcName)),
        args(std::move(args)) {}

  const std::string &getFuncName() const { return funcName; }

  std::vector<std::shared_ptr<const Addr>> getArgs() const {
    return std::vector<std::shared_ptr<const Addr>>{args.begin(), args.end()};
  }

private:
  std::string funcName;
  std::vector<std::shared_ptr<Addr>> args;
};

class Phi : public IRInst, public Definition {
public:
  using Option = std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>;

  Phi(std::shared_ptr<Addr> dest, std::vector<Option> options)
      : Definition(std::move(dest)), options(std::move(options)) {}

  std::vector<
      std::pair<std::shared_ptr<const Addr>, std::shared_ptr<const Label>>>
  getOptions() const {
    std::vector<
        std::pair<std::shared_ptr<const Addr>, std::shared_ptr<const Label>>>
        res;
    for (const auto &option : options)
      res.emplace_back(option.first, option.second);
    return res;
  }

private:
  std::vector<std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>> options;
};

} // namespace ir

} // namespace mocker

#endif // MOCKER_IR_INST_H
