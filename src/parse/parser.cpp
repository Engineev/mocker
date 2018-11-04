#include "parser.h"

#include <cassert>
#include <functional>

#include "ast/ast_node.h"
#include "support/small_map.h"

namespace mocker {

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

namespace mocker {

Parser::Parser(TokIter tokBeg, TokIter tokEnd)
    : tokBeg(tokBeg), tokEnd(tokEnd) {}

std::shared_ptr<ast::Type> Parser::type() { return type(tokBeg, tokEnd); }

std::shared_ptr<ast::Expression> Parser::expression() {
  return expression(tokBeg, tokEnd);
}

std::shared_ptr<ast::ASTRoot> Parser::root() { return root(tokBeg, tokEnd); }

bool Parser::exhausted() const { return tokBeg == tokEnd; }

/*- type ---------------------------------------------------------------------*/

std::shared_ptr<ast::BuiltinType> Parser::builtinType(TokIter &iter,
                                                      TokIter end) {
  auto id = GetTokenID(end)(iter);
  if (id != TokenID::Bool && id != TokenID::Int && id != TokenID::String) {
    return nullptr;
  }

  Position posBeg, posEnd;
  std::tie(posBeg, posEnd) = iter->position();
  auto mk = [&posBeg, &posEnd](ast::BuiltinType::Type ty) {
    return std::make_shared<ast::BuiltinType>(posBeg, posEnd, ty);
  };

  ++iter;
  switch (id) {
  case TokenID::Bool:
    return mk(ast::BuiltinType::Bool);
  case TokenID::Int:
    return mk(ast::BuiltinType::Int);
  case TokenID::String:
    return mk(ast::BuiltinType::String);
  default:
    assert(false);
  }
  assert(false);
}

std::shared_ptr<ast::NonarrayType> Parser::nonarrayType(TokIter &iter,
                                                        TokIter end) {
  if (auto res = builtinType(iter, end))
    return res;
  auto id = GetTokenID(end)(iter);
  if (id == TokenID::Identifier) {
    auto pos = iter->position();
    std::string name = iter->val<std::string>();
    ++iter;
    return std::make_shared<ast::UserDefinedType>(pos.first, pos.second, name);
  }
  return nullptr;
}

std::shared_ptr<ast::Type> Parser::type(TokIter &iter, TokIter end) {
  auto beg = iter;
  std::shared_ptr<ast::Type> res = nonarrayType(iter, end);
  if (!res)
    return nullptr;

  auto id = GetTokenID(end);
  while (id(iter) == TokenID::LeftBracket) {
    ++iter;
    if (id(iter) != TokenID::RightBracket) {
      iter = beg;
      return nullptr;
    }
    res = std::make_shared<ast::ArrayType>(beg->position().first,
                                           iter->position().second, res);
    ++iter;
  }
  return res;
}

/*- expression ---------------------------------------------------------------*/

std::shared_ptr<ast::LiteralExpr> Parser::literalExpr(TokIter &iter,
                                                      TokIter end) {
  auto id = GetTokenID(end)(iter);
  if (id != TokenID::Null && id != TokenID::BoolLit && id != TokenID::IntLit &&
      id != TokenID::StringLit)
    return nullptr;

  auto cur = iter++;
  Position posBeg, posEnd;
  std::tie(posBeg, posEnd) = cur->position();

  switch (id) {
  case TokenID::Null:
    return std::make_shared<ast::NullLitExpr>(posBeg, posEnd);
  case TokenID::BoolLit:
    return std::make_shared<ast::BoolLitExpr>(posBeg, posEnd, cur->val<bool>());
  case TokenID::IntLit:
    return std::make_shared<ast::IntLitExpr>(posBeg, posEnd,
                                             cur->val<Integer>());
  case TokenID::StringLit:
    return std::make_shared<ast::StringLitExpr>(posBeg, posEnd,
                                                cur->val<std::string>());
  default:
    assert(false);
  }
  assert(false);
}

std::shared_ptr<ast::IdentifierExpr> Parser::identifierExpr(TokIter &iter,
                                                            TokIter end) {
  auto id = GetTokenID(end)(iter);
  if (id != TokenID::Identifier)
    return nullptr;
  auto cur = iter++;
  return std::make_shared<ast::IdentifierExpr>(
      cur->position().first, cur->position().second, cur->val<std::string>());
}

std::shared_ptr<ast::NewExpr> Parser::newExpr(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);
  if (id(iter) != TokenID::New)
    return nullptr;
  ++iter;

  std::shared_ptr<ast::Type> baseType;
  std::vector<std::shared_ptr<ast::Expression>> providedDims;
  baseType = nonarrayType(iter, end);
  if (!baseType)
    return nullptr;

  bool noDimNum = false;
  while (id(iter) == TokenID::LeftBracket) {
    ++iter;
    if (id(iter) == TokenID::RightBracket) {
      noDimNum = true;
      ++iter;
      baseType = std::make_shared<ast::ArrayType>(
          begPos, iter->position().second, baseType);
      continue;
    }
    if (noDimNum)
      throw SyntaxError(begPos, iter->position().second);
    auto expr = expression(iter, end);
    if (!expr)
      throw SyntaxError(begPos, iter->position().second);
    providedDims.emplace_back(expr);
    baseType = std::make_shared<ast::ArrayType>(begPos, iter->position().second,
                                                baseType);
    if (id(iter++) != TokenID::RightBracket)
      throw SyntaxError(begPos, iter->position().second);
  }
  return std::make_shared<ast::NewExpr>(begPos, iter->position().second,
                                        baseType, std::move(providedDims));
}

std::shared_ptr<ast::Expression> Parser::primaryExpr(TokIter &iter,
                                                     TokIter end) {
  if (auto res = identifierExpr(iter, end))
    return res;
  if (auto res = literalExpr(iter, end))
    return res;
  if (auto res = newExpr(iter, end))
    return res;

  auto id = GetTokenID(end);
  auto begPos = iter->position().first;
  if (id(iter) != TokenID::LeftParen)
    return nullptr;
  ++iter;
  auto expr = expression(iter, end);
  if (!expr)
    throw SyntaxError(begPos, iter->position().second);
  if (id(iter) != TokenID::RightParen)
    throw SyntaxError(begPos, iter->position().second);
  ++iter;
  return expr;
}

std::shared_ptr<ast::Expression>
Parser::exprPrec2BinaryOrFuncCall(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto throwError = [begPos, &iter] {
    throw SyntaxError(begPos, iter->position().second);
  };

  auto res = primaryExpr(iter, end);
  if (!res)
    return nullptr;
  auto id = GetTokenID(end);
  while (true) {
    if (id(iter) == TokenID::LeftBracket) {
      ++iter;
      auto expr = expression(iter, end);
      if (!expr)
        throwError();
      if (id(iter) != TokenID::RightBracket)
        throwError();
      res = std::make_shared<ast::BinaryExpr>(
          begPos, iter->position().second, ast::BinaryExpr::OpType::Subscript,
          res, expr);
      ++iter;
    } else if (id(iter) == TokenID::Dot) {
      ++iter;
      auto ident = identifierExpr(iter, end);
      if (!ident)
        throwError();
      res = std::make_shared<ast::BinaryExpr>(
          begPos, ident->posEnd, ast::BinaryExpr::Member, res, ident);
    } else if (id(iter) == TokenID::LeftParen) {
      // match the function arguments
      std::vector<std::shared_ptr<ast::Expression>> args;
      if (id(iter + 1) == TokenID::RightParen) {
        ++iter;
        res = std::make_shared<ast::FuncCallExpr>(
            begPos, iter->position().second, res, args);
        ++iter;
        continue;
      }
      do {
        // Now [iter] points to a Comma or a LeftParen
        ++iter;
        auto expr = expression(iter, end);
        if (!expr)
          throwError();
        args.emplace_back(std::move(expr));
      } while (id(iter) == TokenID::Comma);
      if (id(iter) != TokenID::RightParen)
        throwError();
      res = std::make_shared<ast::FuncCallExpr>(begPos, iter->position().second,
                                                res, args);
      ++iter;
    } else {
      break;
    }
  }
  return res;
}

std::shared_ptr<ast::Expression> Parser::suffixIncDec(TokIter &iter,
                                                      TokIter end) {
  auto begPos = iter->position().first;
  std::shared_ptr<ast::Expression> res = exprPrec2BinaryOrFuncCall(iter, end);
  if (!res)
    return nullptr;
  auto id = GetTokenID(end)(iter);
  if (id == TokenID::PlusPlus) {
    res = std::make_shared<ast::UnaryExpr>(begPos, iter->position().second,
                                           ast::UnaryExpr::Inc, res);
    ++iter;
  } else if (id == TokenID::MinusMinus) {
    res = std::make_shared<ast::UnaryExpr>(begPos, iter->position().second,
                                           ast::UnaryExpr::Dec, res);
    ++iter;
  }
  return res;
}

std::shared_ptr<ast::Expression> Parser::prefixUnary(TokIter &iter,
                                                     TokIter end) {
  // ++, --, +, -, !, ~
  // prefix '+' will be ignored
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);
  auto matchOperand = [this, &iter, end, begPos](ast::UnaryExpr::OpType op) {
    ++iter;
    auto expr = suffixIncDec(iter, end);
    if (!expr)
      throw SyntaxError(begPos, iter->position().second);
    return std::make_shared<ast::UnaryExpr>(begPos, expr->posEnd, op,
                                            std::move(expr));
  };

  switch (id(iter)) {
  case TokenID::PlusPlus:
    return matchOperand(ast::UnaryExpr::Inc);
  case TokenID::MinusMinus:
    return matchOperand(ast::UnaryExpr::Dec);
  case TokenID::Plus:
    break;
  case TokenID::Minus:
    return matchOperand(ast::UnaryExpr::Neg);
  case TokenID::LogicalNot:
    return matchOperand(ast::UnaryExpr::LogicalNot);
  case TokenID::BitNot:
    return matchOperand(ast::UnaryExpr::BitNot);
  default:
    break;
  }
  auto expr = suffixIncDec(iter, end);
  return expr;
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
    res =
        std::make_shared<ast::BinaryExpr>(begPos, expr->posEnd, op, res, expr);
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
    res = std::make_shared<ast::BinaryExpr>(begPos, expr->posEnd, mp.second,
                                            res, expr);
  }
  return res;
}

std::shared_ptr<ast::Expression> Parser::multiplicativeExpr(TokIter &iter,
                                                            TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::prefixUnary, this, ph::_1, ph::_2),
                       {{TokenID::Mult, ast::BinaryExpr::Mul},
                        {TokenID::Divide, ast::BinaryExpr::Div},
                        {TokenID::Mod, ast::BinaryExpr::Mod}});
}

std::shared_ptr<ast::Expression> Parser::additiveExpr(TokIter &iter,
                                                      TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(
      iter, end, std::bind(&Parser::multiplicativeExpr, this, ph::_1, ph::_2),
      {{TokenID::Plus, ast::BinaryExpr::Add},
       {TokenID::Minus, ast::BinaryExpr::Sub}});
}

std::shared_ptr<ast::Expression> Parser::shiftExpr(TokIter &iter, TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::additiveExpr, this, ph::_1, ph::_2),
                       {{TokenID::LeftShift, ast::BinaryExpr::Shl},
                        {TokenID::RightShift, ast::BinaryExpr::Shr}});
}

std::shared_ptr<ast::Expression> Parser::relationExpr(TokIter &iter,
                                                      TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::shiftExpr, this, ph::_1, ph::_2),
                       {{TokenID::GreaterThan, ast::BinaryExpr::Gt},
                        {TokenID::GreaterEqual, ast::BinaryExpr::Ge},
                        {TokenID::LessThan, ast::BinaryExpr::Lt},
                        {TokenID::LessEqual, ast::BinaryExpr::Le}});
}

std::shared_ptr<ast::Expression> Parser::equalityExpr(TokIter &iter,
                                                      TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::relationExpr, this, ph::_1, ph::_2),
                       {{TokenID::NotEqual, ast::BinaryExpr::Ne},
                        {TokenID::Equal, ast::BinaryExpr::Eq}});
}

std::shared_ptr<ast::Expression> Parser::bitAndExpr(TokIter &iter,
                                                    TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::equalityExpr, this, ph::_1, ph::_2),
                       {TokenID::BitAnd, ast::BinaryExpr::BitAnd});
}

std::shared_ptr<ast::Expression> Parser::bitXorExpr(TokIter &iter,
                                                    TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::bitAndExpr, this, ph::_1, ph::_2),
                       {TokenID::BitXor, ast::BinaryExpr::Xor});
}
std::shared_ptr<ast::Expression> Parser::bitOrExpr(TokIter &iter, TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::bitXorExpr, this, ph::_1, ph::_2),
                       {TokenID::BitOr, ast::BinaryExpr::BitOr});
}
std::shared_ptr<ast::Expression> Parser::logicalAndExpr(TokIter &iter,
                                                        TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::bitOrExpr, this, ph::_1, ph::_2),
                       {TokenID::LogicalAnd, ast::BinaryExpr::LogicalAnd});
}
std::shared_ptr<ast::Expression> Parser::logicalOrExpr(TokIter &iter,
                                                       TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::logicalAndExpr, this, ph::_1, ph::_2),
                       {TokenID::LogicalOr, ast::BinaryExpr::LogicalOr});
}
std::shared_ptr<ast::Expression> Parser::assignmentExpr(TokIter &iter,
                                                        TokIter end) {
  namespace ph = std::placeholders;
  return auxBinaryExpr(iter, end,
                       std::bind(&Parser::logicalOrExpr, this, ph::_1, ph::_2),
                       {TokenID::Assign, ast::BinaryExpr::Assign});
}

std::shared_ptr<ast::Expression> Parser::expression(TokIter &iter,
                                                    TokIter end) {
  return assignmentExpr(iter, end);
}

/*- statement ----------------------------------------------------------------*/

std::shared_ptr<ast::VarDeclStmt> Parser::varDeclStmt(TokIter &iter,
                                                      TokIter end) {
  auto id = GetTokenID(end);
  auto begPos = iter->position().first;
  auto throwError = [begPos, &iter] {
    throw SyntaxError(begPos, iter->position().second);
  };

  // look ahead
  auto beg = iter;
  if (!type(iter, end) || id(iter++) != TokenID::Identifier ||
      id(iter++) == TokenID::LeftParen) {
    iter = beg;
    return nullptr;
  }
  iter = beg;

  auto varType = type(iter, end);
  assert(varType);
  assert(id(iter) == TokenID::Identifier);
  auto identifier = iter->val<std::string>();
  ++iter;
  if (id(iter) == TokenID::Semicolon)
    return std::make_shared<ast::VarDeclStmt>(
        begPos, (iter++)->position().second, varType, identifier, nullptr);

  if (id(iter) != TokenID::Assign)
    throwError();
  ++iter;
  auto expr = expression(iter, end);
  if (!expr)
    throwError();
  if (id(iter) != TokenID::Semicolon)
    throwError();
  return std::make_shared<ast::VarDeclStmt>(begPos, (iter++)->position().second,
                                            varType, identifier, expr);
}

std::shared_ptr<ast::ExprStmt> Parser::exprStmt(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto expr = expression(iter, end);
  if (!expr)
    return nullptr;
  if (GetTokenID(end)(iter) != TokenID::Semicolon)
    throw SyntaxError(begPos, iter->position().second);
  return std::make_shared<ast::ExprStmt>(begPos, (iter++)->position().second,
                                         expr);
}

std::shared_ptr<ast::ReturnStmt> Parser::returnStmt(TokIter &iter,
                                                    TokIter end) {
  auto begPos = iter->position().first;

  auto id = GetTokenID(end);
  if (id(iter) != TokenID::Return)
    return nullptr;
  ++iter;
  if (id(iter) == TokenID::Semicolon)
    return std::make_shared<ast::ReturnStmt>(
        begPos, (iter++)->position().second, nullptr);
  auto expr = expression(iter, end);
  if (!expr || id(iter) != TokenID::Semicolon)
    throw SyntaxError(begPos, iter->position().second);
  return std::make_shared<ast::ReturnStmt>(begPos, (iter++)->position().second,
                                           expr);
}

std::shared_ptr<ast::ContinueStmt> Parser::continueStmt(TokIter &iter,
                                                        TokIter end) {
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);

  if (id(iter) != TokenID::Continue)
    return nullptr;
  ++iter;
  if (id(iter) != TokenID::Semicolon)
    throw SyntaxError(begPos, iter->position().second);

  return std::make_shared<ast::ContinueStmt>(begPos,
                                             (iter++)->position().second);
}

std::shared_ptr<ast::BreakStmt> Parser::breakStmt(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);

  if (id(iter) != TokenID::Break)
    return nullptr;
  ++iter;
  if (id(iter) != TokenID::Semicolon)
    throw SyntaxError(begPos, iter->position().second);

  return std::make_shared<ast::BreakStmt>(begPos, (iter++)->position().second);
}

std::shared_ptr<ast::CompoundStmt> Parser::compoundStmt(TokIter &iter,
                                                        TokIter end) {
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);

  if (id(iter) != TokenID::LeftBrace)
    return nullptr;
  ++iter;
  std::vector<std::shared_ptr<ast::Statement>> stmts;
  while (auto stmt = statement(iter, end))
    stmts.emplace_back(std::move(stmt));
  if (id(iter) != TokenID::RightBrace)
    throw SyntaxError(begPos, iter->position().second);
  return std::make_shared<ast::CompoundStmt>(
      begPos, (iter++)->position().second, std::move(stmts));
}

std::shared_ptr<ast::IfStmt> Parser::ifStmt(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);

  if (id(iter) != TokenID::If)
    return nullptr;
  ++iter;
  std::shared_ptr<ast::Expression> condition;
  std::shared_ptr<ast::Statement> then;
  if (id(iter++) != TokenID::LeftParen ||
      !(condition = expression(iter, end)) ||
      id(iter++) != TokenID::RightParen || !(then = statement(iter, end)))
    throw SyntaxError(begPos, iter->position().second);
  if (id(iter) != TokenID::Else)
    return std::make_shared<ast::IfStmt>(begPos, iter->position().second,
                                         condition, then, nullptr);
  auto else_ = statement(++iter, end);
  if (!else_)
    throw SyntaxError(begPos, iter->position().second);
  return std::make_shared<ast::IfStmt>(begPos, else_->posEnd, condition, then,
                                       else_);
}

std::shared_ptr<ast::WhileStmt> Parser::whileStmt(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);

  if (id(iter) != TokenID::While)
    return nullptr;
  ++iter;
  std::shared_ptr<ast::Expression> condition;
  std::shared_ptr<ast::Statement> body;
  if (id(iter++) != TokenID::LeftParen ||
      !(condition = expression(iter, end)) ||
      id(iter++) != TokenID::RightParen || !(body = statement(iter, end)))
    throw SyntaxError(begPos, iter->position().second);
  return std::make_shared<ast::WhileStmt>(begPos, body->posEnd, condition,
                                          body);
}

std::shared_ptr<ast::ForStmt> Parser::forStmt(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);
  auto throwError = [begPos, &iter] {
    throw SyntaxError(begPos, iter->position().second);
  };

  if (id(iter) != TokenID::For)
    return nullptr;
  ++iter;
  if (id(iter++) != TokenID::LeftParen)
    throwError();
  auto init = expression(iter, end);
  if (id(iter++) != TokenID::Semicolon)
    throwError();
  auto condition = expression(iter, end);
  if (id(iter++) != TokenID::Semicolon)
    throwError();
  auto update = expression(iter, end);
  if (id(iter++) != TokenID::RightParen)
    throwError();
  auto stmt = statement(iter, end);
  if (!stmt)
    throwError();
  return std::make_shared<ast::ForStmt>(begPos, stmt->posEnd, init, condition,
                                        update, stmt);
}

std::shared_ptr<ast::EmptyStmt> Parser::emptyStmt(TokIter &iter, TokIter end) {
  auto id = GetTokenID(end)(iter);
  if (id != TokenID::Semicolon)
    return nullptr;
  auto begPos = iter->position().first;
  return std::make_shared<ast::EmptyStmt>(begPos, (iter++)->position().second);
}

std::shared_ptr<ast::Statement> Parser::statement(TokIter &iter, TokIter end) {
  std::shared_ptr<ast::Statement> stmt;
  if ((stmt = varDeclStmt(iter, end)) || (stmt = exprStmt(iter, end)) ||
      (stmt = returnStmt(iter, end)) || (stmt = continueStmt(iter, end)) ||
      (stmt = breakStmt(iter, end)) || (stmt = compoundStmt(iter, end)) ||
      (stmt = ifStmt(iter, end)) || (stmt = whileStmt(iter, end)) ||
      (stmt = forStmt(iter, end)) || (stmt = emptyStmt(iter, end)))
    return stmt;
  return nullptr;
}

/*- declaration --------------------------------------------------------------*/

std::shared_ptr<ast::VarDecl> Parser::varDecl(TokIter &iter, TokIter end) {
  if (auto res = varDeclStmt(iter, end))
    return std::make_shared<ast::VarDecl>(res->posBeg, res->posEnd, res);
  return nullptr;
}

std::shared_ptr<ast::FuncDecl> Parser::funcDecl(TokIter &iter, TokIter end) {
  auto beg = iter;
  auto id = GetTokenID(end);
  auto throwError = [beg, &iter] {
    throw SyntaxError(beg->position().first, iter->position().second);
  };

  if (!(type(iter, end) || id(iter++) == TokenID::Void) ||
      id(iter++) != TokenID::Identifier || id(iter++) != TokenID::LeftParen) {
    iter = beg;
    return nullptr;
  }
  iter = beg;
  auto retType = type(iter, end);
  if (!retType) {
    assert(id(iter) == TokenID::Void);
    iter++;
  }
  std::string identifier = (iter++)->val<std::string>();

  std::vector<std::pair<std::shared_ptr<ast::Type>, std::string>> formalParam;
  if (id(iter + 1) != TokenID::RightParen) {
    do {
      // Now [iter] points to a Comma or a LeftParen
      ++iter;
      auto ty = type(iter, end);
      if (!ty)
        throwError();
      if (id(iter) != TokenID::Identifier)
        throwError();
      std::string paramName = (iter++)->val<std::string>();
      formalParam.emplace_back(std::move(ty), std::move(paramName));
    } while (id(iter) == TokenID::Comma);
  } else {
    ++iter;
  }
  ++iter;
  auto body = compoundStmt(iter, end);
  if (!body)
    throwError();
  return std::make_shared<ast::FuncDecl>(beg->position().first, body->posEnd,
                                         retType, identifier,
                                         std::move(formalParam), body);
}

std::shared_ptr<ast::ClassDecl> Parser::classDecl(TokIter &iter, TokIter end) {
  auto id = GetTokenID(end);
  auto begPos = iter->position().first;
  auto throwError = [begPos, &iter] {
    throw SyntaxError(begPos, iter->position().second);
  };

  if (id(iter) != TokenID::Class)
    return nullptr;
  ++iter;
  if (id(iter) != TokenID::Identifier)
    throwError();
  std::string identifier = (iter++)->val<std::string>();
  if (id(iter++) != TokenID::LeftBrace)
    throwError();
  std::vector<std::shared_ptr<ast::Declaration>> members;
  while (true) {
    auto member = declaration(iter, end);
    if (member) {
      members.emplace_back(member);
      continue;
    }
    // ctor
    if (id(iter) == TokenID::Identifier && id(iter + 1) == TokenID::LeftParen &&
        iter->val<std::string>() == identifier) {
      if (iter + 3 >= end)
        throwError();
      auto ctorBegPos = iter->position().first;
      iter += 3;
      auto body = compoundStmt(iter, end);
      if (!body)
        throwError();
      auto ctor = std::make_shared<ast::FuncDecl>(
          ctorBegPos, body->posEnd, std::shared_ptr<ast::Type>(nullptr),
          std::string(""),
          std::vector<std::pair<std::shared_ptr<ast::Type>, std::string>>(),
          body);
      members.emplace_back(ctor);
      continue;
    }
    break;
  }
  if (id(iter) != TokenID::RightBrace)
    throwError();
  return std::make_shared<ast::ClassDecl>(begPos, (iter++)->position().second,
                                          identifier, std::move(members));
}

std::shared_ptr<ast::Declaration> Parser::declaration(TokIter &iter,
                                                      TokIter end) {
  std::shared_ptr<ast::Declaration> decl;
  if ((decl = varDecl(iter, end)) || (decl = funcDecl(iter, end)) ||
      (decl = classDecl(iter, end)))
    return decl;
  return nullptr;
}

/*- root ---------------------------------------------------------------------*/

std::shared_ptr<ast::ASTRoot> Parser::root(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  std::vector<std::shared_ptr<ast::Declaration>> decls;
  while (auto decl = declaration(iter, end)) {
    decls.emplace_back(decl);
  }
  if (decls.empty())
    return nullptr;
  return std::make_shared<ast::ASTRoot>(begPos, decls.back()->posEnd,
                                        std::move(decls));
}

} // namespace mocker