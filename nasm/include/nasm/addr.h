#ifndef MOCKER_NASM_ADDR_H
#define MOCKER_NASM_ADDR_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#define MOCKER_NASM_GET_REG(NAME)                                              \
  inline const std::shared_ptr<Register> &NAME() {                             \
    static auto res = std::make_shared<Register>(#NAME);                       \
    return res;                                                                \
  }

namespace mocker {
namespace nasm {

class Addr {
public:
  virtual ~Addr() = default;
};

class NumericConstant : public Addr {
public:
  explicit NumericConstant(int64_t val) : val(val) {}

  const std::int64_t getVal() const { return val; }

private:
  std::int64_t val;
};

class Register : public Addr {
public:
  explicit Register(std::string identifier)
      : identifier(std::move(identifier)) {}

  const std::string &getIdentifier() const { return identifier; }

private:
  std::string identifier;
};

struct RegPtrHash {
  std::size_t operator()(const std::shared_ptr<Register> &reg) const {
    return std::hash<std::string>{}(reg->getIdentifier());
  }
};

struct RegPtrEqual {
  bool operator()(const std::shared_ptr<Register> &lhs,
                  const std::shared_ptr<Register> &rhs) const {
    return lhs->getIdentifier() == rhs->getIdentifier();
  }
};

class Label : public Addr {
public:
  explicit Label(std::string name) : name(std::move(name)) {}

  const std::string &getName() const { return name; }

private:
  std::string name;
};

class EffectiveAddr : public Addr {};

// [ reg1 + reg2*scale + number ], where scale is 1, 2, 4, or 8 only
class MemoryAddr : public EffectiveAddr {
public:
  explicit MemoryAddr(const std::shared_ptr<Addr> &reg1)
      : reg1(std::static_pointer_cast<Register>(reg1)) {}
  MemoryAddr(const std::shared_ptr<Addr> &reg1, std::int64_t number)
      : reg1(std::static_pointer_cast<Register>(reg1)), number(number) {}

  const std::shared_ptr<Register> &getReg1() const { return reg1; }

  const std::shared_ptr<Register> &getReg2() const { return reg2; }

  const int getScale() const { return scale; }

  const int getNumber() const { return number; }

private:
  std::shared_ptr<Register> reg1 = nullptr, reg2 = nullptr;
  int scale = 0;
  std::int64_t number = 0;
};

class LabelAddr : public EffectiveAddr {
public:
  explicit LabelAddr(std::string labelName) : labelName(std::move(labelName)) {}

  const std::string &getLabelName() const { return labelName; }

private:
  std::string labelName;
};

} // namespace nasm

// physical registers
namespace nasm {

// R0  R1  R2  R3  R4  R5  R6  R7  R8  R9  R10  R11  R12  R13  R14  R15
// RAX RCX RDX RBX RSP RBP RSI RDI

MOCKER_NASM_GET_REG(rax)
MOCKER_NASM_GET_REG(rcx)
MOCKER_NASM_GET_REG(rdx)
MOCKER_NASM_GET_REG(rbx)
MOCKER_NASM_GET_REG(rsp)
MOCKER_NASM_GET_REG(rbp)
MOCKER_NASM_GET_REG(rsi)
MOCKER_NASM_GET_REG(rdi)
MOCKER_NASM_GET_REG(r8)
MOCKER_NASM_GET_REG(r9)
MOCKER_NASM_GET_REG(r10)
MOCKER_NASM_GET_REG(r11)
MOCKER_NASM_GET_REG(r12)
MOCKER_NASM_GET_REG(r13)
MOCKER_NASM_GET_REG(r14)
MOCKER_NASM_GET_REG(r15)

} // namespace nasm

} // namespace mocker

#endif // MOCKER_NASM_ADDR_H
