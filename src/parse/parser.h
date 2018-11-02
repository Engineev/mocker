// A recursive descent parser which produces an unannotated AST

#ifndef MOCKER_PARSER_H
#define MOCKER_PARSER_H

#include "ast/fwd.h"

#include "common.h"
#include <memory>

namespace mocker {

class Parser {
public:
  Parser(TokIter tokBeg, TokIter tokEnd);
  ;

  std::shared_ptr<ast::Type> type();

  std::shared_ptr<ast::Expression> expression();

private:
  TokIter tokBeg, tokEnd;

private: // components
  // All the parser components follows the same conventions:
  // 1. Signature like:
  //      Optional<ASTNode> functionName(TokIter &iter, TokIter end);
  //    where [iter] points to the current token and [end] points to the end of
  //    all the tokens. [ASTNode] is the type of the result this parser may
  //    produce. The result is an empty Optional if a mismatch happened.
  // 2. Throw SyntaxError if it matched only partially. Note that there is
  //    subtle difference between match, mismatch and partial match. For
  //    example, let P = LeftParen IntLiteral RightParen. Then it matches "(1)",
  //    partially matches "(1" and mismatched "[1]".

   /*- type ------------------------------------------------------------------*/

  // builtinType = Int | Bool | String
  std::shared_ptr<ast::BuiltinType> builtinType(TokIter &iter, TokIter end);

  // nonarrayType = builtinType | Identifier
  std::shared_ptr<ast::NonarrayType> nonarrayType(TokIter &iter, TokIter end);

  // type = nonarrayType (LeftBracket RightBracket)*
  std::shared_ptr<ast::Type> type(TokIter &iter, TokIter end);

  /*- expression -------------------------------------------------------------*/

  // literalExpr = Null | BoolLit | IntLit | StringLit
  std::shared_ptr<ast::LiteralExpr> literalExpr(TokIter &iter, TokIter end);

  // identifierExpr = Identifier
  std::shared_ptr<ast::IdentifierExpr> identifierExpr(TokIter &iter,
                                                      TokIter end);

  // newExpr
  //   = New nonarrayType
  //     (LeftBracket expr RightBracket)* (LeftBracket RightBracket)*
  std::shared_ptr<ast::NewExpr> newExpr(TokIter &iter, TokIter end);

  // primaryExpr
  //    = identifierExpr
  //    | literalExpr
  //    | newExpr
  //    | LeftParen expr RightParen
  std::shared_ptr<ast::Expression> primaryExpr(TokIter &iter, TokIter end);

  // exprPrec2BinaryOrFuncCall
  //   = primaryExpr
  //     (
  //     | LeftBracket expr RightBracket
  //     | Dot IdentifierExpr
  //     | LeftParen (expr % ',') RightParen
  //     )*
  std::shared_ptr<ast::Expression> exprPrec2BinaryOrFuncCall(TokIter &iter,
                                                             TokIter end);

  // suffixIncDec
  //   = exprPrec2BinaryOrFuncCall incDecOp?
  std::shared_ptr<ast::Expression> suffixIncDec(TokIter &iter, TokIter end);

  // prefixUnary = unaryOp? suffixIncDec
  std::shared_ptr<ast::Expression> prefixUnary(TokIter &iter, TokIter end);

  // OPERAND (OPERATION OPERAND)*
  std::shared_ptr<ast::Expression> multiplicativeExpr(TokIter &iter,
                                                      TokIter end);
  std::shared_ptr<ast::Expression> additiveExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> shiftExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> relationExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> equalityExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> bitAndExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> bitXorExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> bitOrExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> logicalAndExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> logicalOrExpr(TokIter &iter, TokIter end);
  std::shared_ptr<ast::Expression> assignmentExpr(TokIter &iter, TokIter end);

  std::shared_ptr<ast::Expression> expression(TokIter &iter, TokIter end);
};

} // namespace mocker

#endif // MOCKER_PARSER_H
