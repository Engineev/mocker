#ifndef MOCKER_TOKEN_H
#define MOCKER_TOKEN_H

#include <functional>
#include <memory>
#include <string>
#include <typeinfo>

#include "common/defs.h"

namespace mocker {
namespace detail {

template <class T> struct ValidTokenValType : std::false_type {};

template <> struct ValidTokenValType<Integer> : std::true_type {};

template <> struct ValidTokenValType<std::string> : std::true_type {};

template <> struct ValidTokenValType<bool> : std::true_type {};

template <class T>
void checkTokenType() {
  static_assert(
      detail::ValidTokenValType<std::decay_t<T>>::value,
      "Invalid token value type");
}

} // namespace detail

enum class TokenID {
  Error = 0,
  Identifier,
  // keywords
  Bool,
  Int,
  String,
  Void,
  Null,
  If,
  Else,
  For,
  While,
  Break,
  Continue,
  Return,
  New,
  Class,
  This,
  // literals
  BoolLit,
  StringLit,
  IntLit,
  // punctuation
  LeftParen,
  RightParen,
  LeftBracket,
  RightBracket,
  LeftBrace,
  RightBrace,
  Semicolon,
  Comma,
  // operators
  Plus,
  Minus,
  Mult,
  Divide,
  Mod,
  GreaterThan,
  LessThan,
  Equal,
  NotEqual,
  GreaterEqual,
  LessEqual,
  LogicalNot,
  LogicalAnd,
  LogicalOr,
  LeftShift,
  RightShift,
  BitNot,
  BitAnd,
  BitOr,
  BitXor,
  Assign,
  PlusPlus,
  MinusMinus,
  Dot
};

class Token {
private:
  template <class T> struct Val;

public:
  Token() = default;
  Token(TokenID id, StrIter beg, StrIter end) : id(id), beg(beg), end(end) {}
  template <class T>
  Token(TokenID id, StrIter beg, StrIter end, T &&val)
      : id(id), beg(beg), end(end),
        valPtr(std::make_shared<Val<std::decay_t<T>>>(std::forward<T>(val))) {
    detail::checkTokenType<T>();
  }
  Token(Token &&) noexcept = default;
  Token(const Token &) = default;
  Token &operator=(Token &&) noexcept = default;
  Token &operator=(const Token &) = default;

  // just compare the id and the value
  bool partialCompare(const Token &rhs) const {
    if ((bool)valPtr ^ (bool)rhs.valPtr)
      return false;
    if (!valPtr)
      return true;

    if (type() != rhs.valPtr->type())
      return false;
    if (type() == typeid(Integer))
      return val<Integer>() == rhs.val<Integer>();
    if (type() == typeid(bool))
      return val<bool>() == rhs.val<bool>();
    auto t1 = val<std::string>();
    auto t2 = rhs.val<std::string>();
    return val<std::string>() == rhs.val<std::string>();
  }

  bool operator==(const Token &rhs) const {
    if (beg != rhs.beg || end != rhs.end)
      return false;
    return partialCompare(rhs);
  }

public:
  template <class T> const T &val() const {
    detail::checkTokenType<T>();
    auto ptr = std::dynamic_pointer_cast<Val<T>>(valPtr);
    assert(ptr);
    return ptr->val;
  }

  // return the underlying string
  std::string lexeme() const { return std::string(beg, end); }

  const TokenID tokenID() const {
    assert(valPtr);
    return id;
  }

  bool hasValue() const { return (bool)valPtr; }

  const std::type_info &type() const { return valPtr->type(); }

private:
  TokenID id = TokenID::Error;
  StrIter beg, end; // the begin and end of the token in the source code

  struct ValBase {
    virtual ~ValBase() = default;
    virtual const std::type_info &type() const = 0;
  };

  template <class T> struct Val : ValBase {
    explicit Val(const T &val) : val(val) {
      detail::checkTokenType<T>();
    }
    ~Val() override = default;
    const std::type_info &type() const override { return typeid(val); }

    T val;
  };

  std::shared_ptr<ValBase> valPtr;
};

} // namespace mocker

#endif // MOCKER_TOKEN_H
