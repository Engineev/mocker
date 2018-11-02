#ifndef MOCKER_EXPRESSION_H
#define MOCKER_EXPRESSION_H

#include "common.h"

#include <vector>

namespace mocker {
namespace parse {

bool expression(TokIter & beg, TokIter end);

// identifierExpr = Identifier
bool identifierExpr(TokIter & iter, TokIter end);

// literalExpr = BoolLit | StringLit | IntLit
bool literalExpr(TokIter & iter, TokIter end);

// newExpr =

bool primaryExpr(TokIter & iter, TokIter end);

} // namespace parse
} // namespace mocker

#endif // MOCKER_EXPRESSION_H
