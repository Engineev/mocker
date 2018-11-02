#include "token.h"

namespace mocker {

Token::Token(TokenID id, Position beg, Position end)
    : id(id), beg(beg), end(end) {}

bool Token::partialCompare(const Token &rhs) const {
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

bool Token::operator==(const Token &rhs) const {
  if (beg != rhs.beg || end != rhs.end)
    return false;
  return partialCompare(rhs);
}

bool Token::operator!=(const Token &rhs) const { return !(*this == rhs); }

const TokenID Token::tokenID() const {
  return id;
}

bool Token::hasValue() const { return (bool)valPtr; }

const std::type_info &Token::type() const { return valPtr->type(); }

const std::pair<Position, Position> Token::position() const {
  return {beg, end};
}

} // namespace mocker