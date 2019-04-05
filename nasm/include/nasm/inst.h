#ifndef MOCKER_NASM_INST_H
#define MOCKER_NASM_INST_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "addr.h"

namespace mocker {
// dyc
namespace nasm {

template <class T, class V> std::shared_ptr<T> dyc(V &&v) {
  return std::dynamic_pointer_cast<T>(v);
}

} // namespace nasm

// inst
namespace nasm {

class Inst {
public:
  virtual ~Inst() = default;
};

class Empty : public Inst {};

class Mov : public Inst {
public:
  Mov(std::shared_ptr<Addr> dest, std::shared_ptr<Addr> operand)
      : dest(std::move(dest)), operand(std::move(operand)) {}

  const std::shared_ptr<Addr> getDest() const { return dest; }

  const std::shared_ptr<Addr> getOperand() const { return operand; }

private:
  std::shared_ptr<Addr> dest, operand;
};

class Lea : public Inst {
public:
  Lea(std::shared_ptr<Register> dest, std::shared_ptr<Addr> addr)
      : dest(std::move(dest)), addr(std::move(addr)) {}

  const std::shared_ptr<Register> &getDest() const { return dest; }

  const std::shared_ptr<Addr> &getAddr() const { return addr; }

private:
  std::shared_ptr<Register> dest;
  std::shared_ptr<Addr> addr;
};

class UnaryInst : public Inst {
public:
  enum OpType { Neg, Not, Inc, Dec };

  UnaryInst(OpType op, std::shared_ptr<Register> reg)
      : op(op), reg(std::move(reg)) {}

  OpType getOp() const { return op; }

  const std::shared_ptr<Register> &getReg() { return reg; }

private:
  OpType op;
  std::shared_ptr<Register> reg;
};

class BinaryInst : public Inst {
public:
  enum OpType { BitOr, BitAnd, Xor, Add, Sub, Mul, Sal, Sar };

  BinaryInst(OpType type, std::shared_ptr<Addr> lhs, std::shared_ptr<Addr> rhs)
      : type(type), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

  const OpType getType() const { return type; }

  const std::shared_ptr<Addr> &getLhs() const { return lhs; }

  const std::shared_ptr<Addr> &getRhs() const { return rhs; }

private:
  OpType type;
  std::shared_ptr<Addr> lhs, rhs;
};

class Ret : public Inst {};

class Call : public Inst {
public:
  explicit Call(std::string funcName) : funcName(std::move(funcName)) {}

  const std::string &getFuncName() const { return funcName; }

private:
  std::string funcName;
};

class Db : public Inst {
public:
  explicit Db(std::string data) : data(std::move(data)) {}

  const std::string &getData() const { return data; }

private:
  std::string data;
};

class Resb : public Inst {
public:
  explicit Resb(size_t sz) : sz(sz) {}

  const std::size_t getSize() const { return sz; }

private:
  std::size_t sz;
};

class Push : public Inst {
public:
  explicit Push(std::shared_ptr<Register> reg) : reg(std::move(reg)) {}

  const std::shared_ptr<Register> &getReg() const { return reg; }

private:
  std::shared_ptr<Register> reg;
};

class Pop : public Inst {
public:
  explicit Pop(std::shared_ptr<Register> reg) : reg(std::move(reg)) {}

  const std::shared_ptr<Register> &getReg() const { return reg; }

private:
  std::shared_ptr<Register> reg;
};

class Leave : public Inst {};

class Jmp : public Inst {
public:
  explicit Jmp(std::shared_ptr<Label> label) : label(std::move(label)) {}

  const std::shared_ptr<Label> &getLabel() const { return label; }

private:
  std::shared_ptr<Label> label;
};

class Cmp : public Inst {
public:
  Cmp(std::shared_ptr<Addr> lhs, std::shared_ptr<Addr> rhs)
      : lhs(std::move(lhs)), rhs(std::move(rhs)) {}

  const std::shared_ptr<Addr> &getLhs() const { return lhs; }

  const std::shared_ptr<Addr> &getRhs() const { return rhs; }

private:
  std::shared_ptr<Addr> lhs, rhs;
};

class Set : public Inst {
public:
  enum OpType { Eq, Ne, Lt, Le, Gt, Ge };

  Set(OpType op, std::shared_ptr<Register> reg) : op(op), reg(std::move(reg)) {}

  OpType getOp() const { return op; }

  const std::shared_ptr<Register> &getReg() const { return reg; }

private:
  OpType op;
  std::shared_ptr<Register> reg;
};

class CJump : public Inst {
public:
  enum OpType {
    Nz,
  };

  CJump(OpType op, std::shared_ptr<Label> label)
      : op(op), label(std::move(label)) {}

  OpType getOp() const { return op; }

  const std::shared_ptr<Label> &getLabel() const { return label; }

private:
  OpType op;
  std::shared_ptr<Label> label;
};

class IDivq : public Inst {
public:
  explicit IDivq(std::shared_ptr<Addr> rhs) : rhs(std::move(rhs)) {}

  const std::shared_ptr<Addr> &getRhs() const { return rhs; }

private:
  std::shared_ptr<Addr> rhs;
};

class Cqto : public Inst {};

} // namespace nasm

// directive
namespace nasm {

class Directive {
public:
  virtual ~Directive() = default;
};

class Default : public Directive {
public:
  explicit Default(std::string mode) : mode(std::move(mode)) {}

  const std::string &getMode() const { return mode; }

private:
  std::string mode;
};

class Global : public Directive {
public:
  explicit Global(std::string identifier) : identifier(std::move(identifier)) {}

  const std::string &getIdentifier() const { return identifier; }

private:
  std::string identifier;
};

class Extern : public Directive {
public:
  explicit Extern(std::string identifier) : identifier(std::move(identifier)) {}

  const std::string getIdentifier() const { return identifier; }

private:
  std::string identifier;
};

} // namespace nasm

// pseudo inst
namespace nasm {

class AlignStack : public Inst {};

} // namespace nasm

} // namespace mocker

#endif // MOCKER_NASM_INST_H
