#ifndef MOCKER_IR_INST_H
#define MOCKER_IR_INST_H

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <string>
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

struct IRInst : std::enable_shared_from_this<IRInst> {
  virtual ~IRInst() = default;

  InstID getID() const { return (InstID)this; }
};

struct Terminator {};

struct Deleted : IRInst {};

struct Comment : IRInst {
  explicit Comment(std::string str) : str(std::move(str)) {}
  explicit Comment(const char *str) : str(str) {}

  std::string str;
};

// When printing the IR, a comment of this type will be attached right after the
// next instruction.
struct AttachedComment : IRInst {
  explicit AttachedComment(std::string str) : str(std::move(str)) {}
  explicit AttachedComment(const char *str) : str(str) {}

  std::string str;
};

struct Assign : IRInst {
  Assign(std::shared_ptr<Addr> dest, std::shared_ptr<Addr> operand)
      : dest(std::move(dest)), operand(std::move(operand)) {}
  std::shared_ptr<Addr> dest;
  std::shared_ptr<Addr> operand;
};

struct ArithUnaryInst : IRInst {
  enum OpType { Neg, BitNot };

  ArithUnaryInst(std::shared_ptr<Addr> dest, OpType op,
                 std::shared_ptr<Addr> operand)
      : op(op), dest(std::move(dest)), operand(std::move(operand)) {}

  OpType op;
  std::shared_ptr<Addr> dest;
  std::shared_ptr<Addr> operand;
};

struct ArithBinaryInst : IRInst {
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
      : dest(std::move(dest)), op(op), lhs(std::move(lhs)),
        rhs(std::move(rhs)) {}

  std::shared_ptr<Addr> dest;
  OpType op;
  std::shared_ptr<Addr> lhs, rhs;
};

struct RelationInst : IRInst {
  enum OpType { Eq, Ne, Lt, Gt, Le, Ge };

  RelationInst(std::shared_ptr<Addr> dest, OpType op, std::shared_ptr<Addr> lhs,
               std::shared_ptr<Addr> rhs)
      : dest(std::move(dest)), op(op), lhs(std::move(lhs)),
        rhs(std::move(rhs)) {}

  std::shared_ptr<Addr> dest;
  OpType op;
  std::shared_ptr<Addr> lhs, rhs;
};

struct Store : IRInst {
  Store(std::shared_ptr<Addr> dest, std::shared_ptr<Addr> operand)
      : dest(std::move(dest)), operand(std::move(operand)) {}

  std::shared_ptr<Addr> dest;
  std::shared_ptr<Addr> operand;
};

struct Load : IRInst {
  Load(std::shared_ptr<Addr> dest, std::shared_ptr<Addr> addr)
      : dest(std::move(dest)), addr(std::move(addr)) {}

  std::shared_ptr<Addr> dest;
  std::shared_ptr<Addr> addr;
};

struct Alloca : IRInst {
  Alloca(std::shared_ptr<Addr> dest, size_t size)
      : dest(std::move(dest)), size(size) {}

  std::shared_ptr<Addr> dest;
  std::size_t size;
};

struct Malloc : IRInst {
  Malloc(std::shared_ptr<Addr> dest, std::shared_ptr<Addr> size)
      : dest(std::move(dest)), size(std::move(size)) {}

  std::shared_ptr<Addr> dest, size;
};

struct SAlloc : IRInst {
  SAlloc(std::shared_ptr<Addr> dest, size_t size)
      : dest(std::move(dest)), size(size) {}

  std::shared_ptr<Addr> dest;
  std::size_t size;
};

struct StrCpy : IRInst {
  StrCpy(std::shared_ptr<Addr> dest, std::string data)
      : dest(std::move(dest)), data(std::move(data)) {}

  std::shared_ptr<Addr> dest;
  std::string data;
};

struct Branch : IRInst, Terminator {
  Branch(std::shared_ptr<Addr> condition, std::shared_ptr<Label> then,
         std::shared_ptr<Label> else_)
      : condition(std::move(condition)), then(std::move(then)),
        else_(std::move(else_)) {}

  std::shared_ptr<Addr> condition;
  std::shared_ptr<Label> then, else_;
};

struct Jump : IRInst, Terminator {
  explicit Jump(std::shared_ptr<Label> dest) : dest(std::move(dest)) {}

  std::shared_ptr<Label> dest;
};

struct Ret : IRInst, Terminator {
  explicit Ret(std::shared_ptr<Addr> val = nullptr) : val(std::move(val)) {}

  std::shared_ptr<Addr> val;
};

struct Call : IRInst {
  template <class... Args>
  Call(std::shared_ptr<Addr> dest, std::string funcName, Args... args)
      : dest(std::move(dest)), funcName(std::move(funcName)),
        args({std::move(args)...}) {}

  template <class... Args>
  explicit Call(std::string funcName, Args... args)
      : dest(nullptr), funcName(std::move(funcName)),
        args({std::move(args)...}) {}

  Call(std::shared_ptr<Addr> dest, std::string funcName,
       std::vector<std::shared_ptr<Addr>> args)
      : dest(std::move(dest)), funcName(std::move(funcName)),
        args(std::move(args)) {}

  std::shared_ptr<Addr> dest;
  std::string funcName;
  std::vector<std::shared_ptr<Addr>> args;
};

struct Phi : IRInst {
  using Option = std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>;

  Phi(std::shared_ptr<Addr> dest, std::vector<Option> options)
      : dest(std::move(dest)), options(std::move(options)) {}

  std::shared_ptr<Addr> dest;
  std::vector<std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>> options;
};

} // namespace ir

} // namespace mocker

#endif // MOCKER_IR_INST_H
