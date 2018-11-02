#ifndef MOCKER_COMMON_H
#define MOCKER_COMMON_H

#include <exception>
#include <vector>

#include "common/defs.h"
#include "common/error.h"
#include "lex/token.h"

namespace mocker {

class SyntaxError : public CompileError {
public:
  SyntaxError(const Position &beg, const Position &end)
    : CompileError(beg, end) {}
};

using TokIter = std::vector<Token>::const_iterator;

class GetTokenID {
public:
  explicit GetTokenID(TokIter end) : end(end) {}

  TokenID operator()(TokIter iter) const {
    if (iter == end)
      return TokenID::Error;
    return iter->tokenID();
  }

private:
  TokIter end;
};

} // namespace mocker

#endif // MOCKER_COMMON_H
