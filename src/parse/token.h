#ifndef MOCKER_TOKEN_H
#define MOCKER_TOKEN_H

#include <cassert>
#include <functional>
#include <memory>
#include <string>
#include <typeinfo>

#include "common/defs.h"
#include "common/position.h"

namespace mocker {
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

inline bool isLiteral(TokenID id) {
  return TokenID::BoolLit <= id && id <= TokenID::IntLit;
}

} // namespace mocker

namespace mocker {

namespace detail {

template <class T> struct ValidTokenValType : std::false_type {};

template <> struct ValidTokenValType<Integer> : std::true_type {};

template <> struct ValidTokenValType<std::string> : std::true_type {};

template <> struct ValidTokenValType<bool> : std::true_type {};

template <class T> void checkTokenType() {
  static_assert(detail::ValidTokenValType<std::decay_t<T>>::value,
                "Invalid token value type");
}

} // namespace detail

class Token {
private:
  template <class T> struct Val;

public:
  Token() = default;
  Token(TokenID id, Position beg, Position end);

  template <class T>
  Token(TokenID id, Position beg, Position end, T &&val);

  Token(Token &&) noexcept = default;
  Token(const Token &) = default;
  Token &operator=(Token &&) noexcept = default;
  Token &operator=(const Token &) = default;

  // just compare the id and the value
  bool partialCompare(const Token &rhs) const;

  bool operator==(const Token &rhs) const;

  bool operator!=(const Token &rhs) const;

public:
  template <class T> const T &val() const {
    detail::checkTokenType<T>();
    auto ptr = std::dynamic_pointer_cast<Val<T>>(valPtr);
    assert(ptr);
    return ptr->val;
  }

  const TokenID tokenID() const;

  bool hasValue() const;

  const std::type_info &type() const;

  const std::pair<Position, Position> position() const;

private:
  TokenID id = TokenID::Error;
  Position beg, end;

  struct ValBase {
    virtual ~ValBase() = default;
    virtual const std::type_info &type() const = 0;
  };

  template <class T> struct Val : ValBase {
    explicit Val(const T &val) : val(val) { detail::checkTokenType<T>(); }
    ~Val() override = default;
    const std::type_info &type() const override { return typeid(val); }

    T val;
  };

  std::shared_ptr<ValBase> valPtr;
};

template<class T>
Token::Token(TokenID id, Position beg, Position end, T &&val)
    : id(id), beg(beg), end(end),
      valPtr(std::make_shared<Val<std::decay_t<T>>>(std::forward<T>(val))) {
  detail::checkTokenType<T>();
}

} // namespace mocker

#endif // MOCKER_TOKEN_H
