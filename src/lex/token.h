#ifndef MOCKER_TOKEN_H
#define MOCKER_TOKEN_H

#include <functional>
#include <memory>
#include <string>
#include <typeinfo>

#include "common/defs.h"

namespace mocker {

enum class TokenID {
  Error = 0,
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
  Token(TokenID id, StrIter beg, StrIter end, T && val)
    : id(id), beg(beg), end(end),
    valPtr(std::make_unique<Val<T>>(std::forward<T>(val))) {}
  Token(Token &&) noexcept = default;
  Token &operator=(Token &&) noexcept = default;

  template <class T> const T &val() const {
    assert(valPtr);
    auto ptr = dynamic_cast<Val<T> *>(&*valPtr);
    return ptr->val;
  }

  // return the underlying string
  std::string lexeme() const { return std::string(beg, end); }

  const TokenID tokenID() const {
    return id;
  }

private:
  TokenID id = TokenID::Error;
  StrIter beg, end; // the begin and end of the token in the source code

  struct ValBase {
    virtual ~ValBase() = default;
  };
  template <class T> struct Val : ValBase {
    explicit Val(const T &val) : val(val) {}
    ~Val() override = default;
    T val;
  };

  std::unique_ptr<ValBase> valPtr;
};

} // namespace mocker

#endif // MOCKER_TOKEN_H
