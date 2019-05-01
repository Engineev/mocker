#ifndef MOCKER_IR_INST_H
#define MOCKER_IR_INST_H

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mocker {
// Addr
namespace ir {

class Addr {
public:
  virtual ~Addr() = default;
};

class IntLiteral : public Addr {
public:
  explicit IntLiteral(std::int64_t val) : val(val) {}

  const std::int64_t getVal() const { return val; }

private:
  std::int64_t val;
};

class Reg : public Addr {
public:
  explicit Reg(std::string identifier) : identifier(std::move(identifier)) {}

  const std::string &getIdentifier() const { return identifier; }

private:
  std::string identifier;
};

struct RegPtrHash {
  std::size_t operator()(const std::shared_ptr<ir::Reg> &reg) const {
    return std::hash<std::string>{}(reg->getIdentifier());
  }
};

struct RegPtrEqual {
  bool operator()(const std::shared_ptr<ir::Reg> &lhs,
                  const std::shared_ptr<ir::Reg> &rhs) const {
    return lhs->getIdentifier() == rhs->getIdentifier();
  }
};

template <class T>
using RegMap =
    std::unordered_map<std::shared_ptr<ir::Reg>, T, RegPtrHash, RegPtrEqual>;
using RegSet =
    std::unordered_set<std::shared_ptr<ir::Reg>, RegPtrHash, RegPtrEqual>;

class Label : public Addr {
public:
  explicit Label(size_t id) : id(id) {}

  const std::size_t getID() const { return id; }

private:
  std::size_t id;
};

} // namespace ir

// IRInst
namespace ir {

using InstID = std::uintptr_t;

class IRInst {
public:
  enum InstType {
    Deleted,
    Comment,
    AttachedComment,
    Assign,
    ArithUnaryInst,
    ArithBinaryInst,
    RelationInst,
    Store,
    Load,
    Alloca,
    Malloc,
    Branch,
    Jump,
    Ret,
    Call,
    Phi
  };

  explicit IRInst(InstType type) : type(type) {}

  virtual ~IRInst() = default;

  InstID getID() const { return (InstID)this; }

  InstType getInstType() const { return type; }

private:
  InstType type;
};

class Terminator {
public:
  virtual ~Terminator() = default;
};

class Definition {
public:
  explicit Definition(std::shared_ptr<Reg> dest) : dest(std::move(dest)) {}

  virtual ~Definition() = default;

  const std::shared_ptr<Reg> &getDest() const { return dest; }

protected:
  std::shared_ptr<Reg> dest;
};

class Deleted : public IRInst {
public:
  Deleted() : IRInst(IRInst::Deleted) {}
};

class Comment : public IRInst {
public:
  explicit Comment(std::string str)
      : IRInst(InstType::Comment), str(std::move(str)) {}
  explicit Comment(const char *str) : IRInst(InstType::Comment), str(str) {}

  const std::string &getContent() const { return str; }

private:
  std::string str;
};

// When printing the IR, a comment of this type will be attached right after the
// next instruction.
class AttachedComment : public IRInst {
public:
  explicit AttachedComment(std::string str)
      : IRInst(InstType::AttachedComment), str(std::move(str)) {}
  explicit AttachedComment(const char *str)
      : IRInst(InstType::AttachedComment), str(str) {}

  const std::string &getContent() const { return str; }

private:
  std::string str;
};

class Assign : public IRInst, public Definition {
public:
  Assign(std::shared_ptr<Reg> dest, std::shared_ptr<Addr> operand)
      : IRInst(InstType::Assign), Definition(std::move(dest)),
        operand(std::move(operand)) {}

  const std::shared_ptr<Addr> &getOperand() const { return operand; }

private:
  std::shared_ptr<Addr> operand;
};

class ArithUnaryInst : public IRInst, public Definition {
public:
  enum OpType { Neg, BitNot };

  ArithUnaryInst(std::shared_ptr<Reg> dest, OpType op,
                 std::shared_ptr<Addr> operand)
      : IRInst(InstType::ArithUnaryInst), Definition(std::move(dest)), op(op),
        operand(std::move(operand)) {}

  const OpType getOp() const { return op; }

  const std::shared_ptr<Addr> &getOperand() const { return operand; }

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

  ArithBinaryInst(std::shared_ptr<Reg> dest, OpType op,
                  std::shared_ptr<Addr> lhs, std::shared_ptr<Addr> rhs)
      : IRInst(InstType::ArithBinaryInst), Definition(std::move(dest)), op(op),
        lhs(std::move(lhs)), rhs(std::move(rhs)) {}

  const OpType getOp() const { return op; }

  const std::shared_ptr<Addr> &getLhs() const { return lhs; }

  const std::shared_ptr<Addr> &getRhs() const { return rhs; }

private:
  OpType op;
  std::shared_ptr<Addr> lhs, rhs;
};

class RelationInst : public IRInst, public Definition {
public:
  enum OpType { Eq, Ne, Lt, Gt, Le, Ge };

  RelationInst(std::shared_ptr<Reg> dest, OpType op, std::shared_ptr<Addr> lhs,
               std::shared_ptr<Addr> rhs)
      : IRInst(InstType::RelationInst), Definition(std::move(dest)), op(op),
        lhs(std::move(lhs)), rhs(std::move(rhs)) {}

  const OpType getOp() const { return op; }

  const std::shared_ptr<Addr> &getLhs() const { return lhs; }

  const std::shared_ptr<Addr> &getRhs() const { return rhs; }

private:
  OpType op;
  std::shared_ptr<Addr> lhs, rhs;
};

class Store : public IRInst {
public:
  Store(std::shared_ptr<Addr> addr, std::shared_ptr<Addr> val)
      : IRInst(InstType::Store), addr(std::move(addr)), val(std::move(val)) {}

  const std::shared_ptr<Addr> &getAddr() const { return addr; }

  const std::shared_ptr<Addr> &getVal() const { return val; }

private:
  std::shared_ptr<Addr> addr;
  std::shared_ptr<Addr> val;
};

class Load : public IRInst, public Definition {
public:
  Load(std::shared_ptr<Reg> dest, std::shared_ptr<Addr> addr)
      : IRInst(InstType::Load), Definition(std::move(dest)),
        addr(std::move(addr)) {}

  const std::shared_ptr<Addr> &getAddr() const { return addr; }

private:
  std::shared_ptr<Addr> addr;
};

class Alloca : public IRInst, public Definition {
public:
  explicit Alloca(std::shared_ptr<Reg> dest)
      : IRInst(InstType::Alloca), Definition(std::move(dest)) {}
};

class Malloc : public IRInst, public Definition {
public:
  Malloc(std::shared_ptr<Reg> dest, std::shared_ptr<Addr> size)
      : IRInst(InstType::Malloc), Definition(std::move(dest)),
        size(std::move(size)) {}

  const std::shared_ptr<Addr> &getSize() const { return size; }

private:
  std::shared_ptr<Addr> size;
};

class Branch : public IRInst, public Terminator {
public:
  Branch(std::shared_ptr<Addr> condition, std::shared_ptr<Label> then,
         std::shared_ptr<Label> else_)
      : IRInst(InstType::Branch), condition(std::move(condition)),
        then(std::move(then)), else_(std::move(else_)) {}

  const std::shared_ptr<Addr> &getCondition() const { return condition; }

  const std::shared_ptr<Label> &getThen() const { return then; }

  const std::shared_ptr<Label> &getElse() const { return else_; }

private:
  std::shared_ptr<Addr> condition;
  std::shared_ptr<Label> then, else_;
};

class Jump : public IRInst, public Terminator {
public:
  explicit Jump(std::shared_ptr<Label> label)
      : IRInst(InstType::Jump), label(std::move(label)) {}

  const std::shared_ptr<Label> &getLabel() const { return label; }

private:
  std::shared_ptr<Label> label;
};

class Ret : public IRInst, public Terminator {
public:
  explicit Ret(std::shared_ptr<Addr> val = nullptr)
      : IRInst(InstType::Ret), val(std::move(val)) {}

  const std::shared_ptr<Addr> &getVal() const { return val; }

private:
  std::shared_ptr<Addr> val;
};

class Call : public IRInst, public Definition {
public:
  template <class... Args>
  Call(std::shared_ptr<Reg> dest, std::string funcName, Args... args)
      : IRInst(InstType::Call), Definition(std::move(dest)),
        funcName(std::move(funcName)), args({std::move(args)...}) {}

  template <class... Args>
  explicit Call(std::string funcName, Args... args)
      : IRInst(InstType::Call), Definition(nullptr),
        funcName(std::move(funcName)), args({std::move(args)...}) {}

  Call(std::shared_ptr<Reg> dest, std::string funcName,
       std::vector<std::shared_ptr<Addr>> args)
      : IRInst(InstType::Call), Definition(std::move(dest)),
        funcName(std::move(funcName)), args(std::move(args)) {}

  const std::string &getFuncName() const { return funcName; }

  const std::vector<std::shared_ptr<Addr>> &getArgs() const { return args; }

private:
  std::string funcName;
  std::vector<std::shared_ptr<Addr>> args;
};

class Phi : public IRInst, public Definition {
public:
  using Option = std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>;

  Phi(std::shared_ptr<Reg> dest, std::vector<Option> options)
      : IRInst(InstType::Phi), Definition(std::move(dest)),
        options(std::move(options)) {}

  const std::vector<Option> &getOptions() const { return options; }

private:
  std::vector<std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>> options;
};

} // namespace ir

} // namespace mocker

#endif // MOCKER_IR_INST_H
