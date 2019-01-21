// A recursive descent parser which produces an unannotated AST

#ifndef MOCKER_PARSER_H
#define MOCKER_PARSER_H

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast/ast_node.h"
#include "common/error.h"
#include "common/position.h"
#include "small_map.h"
#include "token.h"

namespace mocker {

class SyntaxError : public CompileError {
public:
  SyntaxError(const Position &beg, const Position &end)
      : CompileError(beg, end) {}
};

using TokIter = std::vector<Token>::const_iterator;

class Parser {
public:
  Parser(TokIter tokBeg, TokIter tokEnd,
         std::unordered_map<ast::NodeID, PosPair> &pos);

  std::shared_ptr<ast::Type> type();

  std::shared_ptr<ast::Expression> expression();

  std::shared_ptr<ast::ASTRoot> root();

  bool exhausted() const;

private:
  TokIter tokBeg, tokEnd;
  std::unordered_map<ast::NodeID, PosPair> &pos;

private: // helpers
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

  template <class Node, class... Args>
  std::shared_ptr<Node> makeNode(Position beg, Position end, Args &&... args) {
    auto res = std::make_shared<Node>(std::forward<Args>(args)...);
    pos[res->getID()] = std::make_pair(beg, end);
    return res;
  }

  template <class Node, class... Args>
  std::shared_ptr<Node> makeNode(PosPair p, Args &&... args) {
    return makeNode<Node>(p.first, p.second, std::forward<Args>(args)...);
  }

  template <class OperandParser>
  std::shared_ptr<ast::Expression>
  auxBinaryExpr(TokIter &iter, TokIter end, OperandParser operand,
                const SmallMap<TokenID, ast::BinaryExpr::OpType> &mp) {
    auto begPos = iter->position().first;
    auto res = operand(iter, end);
    if (!res)
      return nullptr;

    auto id = GetTokenID(end);
    while (mp.in(id(iter))) {
      ast::BinaryExpr::OpType op = mp.at(id(iter));
      ++iter;
      auto expr = operand(iter, end);
      if (!expr)
        throw SyntaxError(begPos, iter->position().second);
      res = makeNode<ast::BinaryExpr>(begPos, pos[expr->getID()].second, op,
                                      res, expr);
    }
    return res;
  }

  template <class OperandParser>
  std::shared_ptr<ast::Expression>
  auxBinaryExpr(TokIter &iter, TokIter end, OperandParser operand,
                const std::pair<TokenID, ast::BinaryExpr::OpType> &mp) {
    auto begPos = iter->position().first;
    auto res = operand(iter, end);
    if (!res)
      return nullptr;

    auto id = GetTokenID(end);
    while (id(iter) == mp.first) {
      ++iter;
      auto expr = operand(iter, end);
      if (!expr)
        throw SyntaxError(begPos, iter->position().second);
      res = makeNode<ast::BinaryExpr>(begPos, pos[expr->getID()].second,
                                      mp.second, res, expr);
    }
    return res;
  }

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

  /*- misc -------------------------------------------------------------------*/

  // identifier = Identifier
  std::shared_ptr<ast::Identifier> identifier(TokIter &iter, TokIter end);

  // builtinType = Int | Bool | String
  std::shared_ptr<ast::BuiltinType> builtinType(TokIter &iter, TokIter end);

  // nonarrayType = builtinType | identifier
  std::shared_ptr<ast::NonarrayType> nonarrayType(TokIter &iter, TokIter end);

  // type = nonarrayType (LeftBracket RightBracket)*
  // look ahead ?
  std::shared_ptr<ast::Type> type(TokIter &iter, TokIter end);

  /*- expression -------------------------------------------------------------*/

  // literalExpr = Null | BoolLit | IntLit | StringLit
  std::shared_ptr<ast::LiteralExpr> literalExpr(TokIter &iter, TokIter end);

  // identifierExpr = Identifier
  std::shared_ptr<ast::IdentifierExpr> identifierExpr(TokIter &iter,
                                                      TokIter end);

  // newExpr
  //   = New nonarrayType
  //     (
  //       LeftParen RightParen
  //     | (LeftBracket expr RightBracket)* (LeftBracket RightBracket)*
  //     )
  std::shared_ptr<ast::NewExpr> newExpr(TokIter &iter, TokIter end);

  // primaryExpr
  //    = identifierExpr // some lookahead should be perform here
  //    | identifier LeftParen (expr % ',') RightParen
  //    | literalExpr
  //    | newExpr
  //    | LeftParen expr RightParen
  std::shared_ptr<ast::Expression> primaryExpr(TokIter &iter, TokIter end);

  // exprPrec2BinaryOrFuncCall
  //   = primaryExpr
  //     (
  //     | LeftBracket expr RightBracket
  //     | Dot IdentifierExpr
  //       // Some lookahead should be perform here
  //       // Actually I don't think a.(a) is valid in Mx*, but to pass the
  //       // semantic test, I allow it to be valid.
  //     | Dot IdentifierExpr LeftParen (expr % ',') RightParen
  //     )*
  std::shared_ptr<ast::Expression> exprPrec2BinaryOrFuncCall(TokIter &iter,
                                                             TokIter end);

  // suffixIncDec
  //   = exprPrec2BinaryOrFuncCall incDecOp?
  std::shared_ptr<ast::Expression> suffixIncDec(TokIter &iter, TokIter end);

  // prefixUnary = unaryOp* suffixIncDec
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

  /*- statement --------------------------------------------------------------*/

  // varDeclStmt
  //   = &(type identifier) // look ahead
  //     type identifier (Assign expr)? Semicolon
  std::shared_ptr<ast::VarDeclStmt> varDeclStmt(TokIter &iter, TokIter end);

  // exprStmt = expr Semicolon
  std::shared_ptr<ast::ExprStmt> exprStmt(TokIter &iter, TokIter end);

  // returnStmt = Return expr? Semicolon
  std::shared_ptr<ast::ReturnStmt> returnStmt(TokIter &iter, TokIter end);

  // continueStmt = Continue Semicolon
  std::shared_ptr<ast::ContinueStmt> continueStmt(TokIter &iter, TokIter end);

  // breakStmt = Break Semicolon
  std::shared_ptr<ast::BreakStmt> breakStmt(TokIter &iter, TokIter end);

  // compoundStmt = LeftBrace statement* RightBrace
  std::shared_ptr<ast::CompoundStmt> compoundStmt(TokIter &iter, TokIter end);

  // ifStmt = If LeftParen expr RightParen statement (Else stmt)?
  std::shared_ptr<ast::IfStmt> ifStmt(TokIter &iter, TokIter end);

  // whileStmt = While LeftParen expr RightParen stmt
  std::shared_ptr<ast::WhileStmt> whileStmt(TokIter &iter, TokIter end);

  // forStmt
  //   = For LeftParen expr? Semicolon expr? Semicolon expr? RightParen stmt
  std::shared_ptr<ast::ForStmt> forStmt(TokIter &iter, TokIter end);

  // emptyStmt = Semicolon
  std::shared_ptr<ast::EmptyStmt> emptyStmt(TokIter &iter, TokIter end);

  std::shared_ptr<ast::Statement> statement(TokIter &iter, TokIter end);

  /*- declaration ------------------------------------------------------------*/

  // varDec = varDeclStmt
  std::shared_ptr<ast::VarDecl> varDecl(TokIter &iter, TokIter end);

  // funcDecl
  //   = &((type | Void) identifier LeftParen) // look ahead
  //     (type | Void) identifier
  //     LeftParen ((type identifier) % Comma) RightParen
  std::shared_ptr<ast::FuncDecl> funcDecl(TokIter &iter, TokIter end);

  // classDecl = Class identifier LeftBrace (declaration | ctor)* RightBrace
  // ctor is inlined. If a function does not have a return type and its
  // identifier is same as the class identifier, then it is renamed to
  // _ctor_[identifier].
  std::shared_ptr<ast::ClassDecl> classDecl(TokIter &iter, TokIter end);

  std::shared_ptr<ast::Declaration> declaration(TokIter &iter, TokIter end);

  /*- root -------------------------------------------------------------------*/
  std::shared_ptr<ast::ASTRoot> root(TokIter &iter, TokIter end);

  /*- helper -----------------------------------------------------------------*/
  std::vector<std::shared_ptr<ast::Expression>> exprList(TokIter &iter,
                                                         TokIter end);
};

} // namespace mocker

#endif // MOCKER_PARSER_H
