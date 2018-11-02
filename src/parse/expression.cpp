#include "expression.h"

namespace mocker {
namespace parse {

bool primaryExpr(TokIter &iter, TokIter end) {
  auto id = GetTokenID(end);
  if (id(iter) == TokenID::Identifier) {
    return true;
  }
  if (isLiteral(id(iter))) {
    ++iter;
    return true;
  }
  if (id(iter) == TokenID::LeftParen) {


  }
  return false;
}

} // namespace parse
} // namespace mocker
