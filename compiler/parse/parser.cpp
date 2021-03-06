#include "parser.h"

#include <cassert>
#include <functional>

#include "ast/ast_node.h"
#include "small_map.h"

namespace mocker {

Parser::Parser(TokIter tokBeg, TokIter tokEnd,
               std::unordered_map<ast::NodeID, PosPair> &pos)
    : tokBeg(tokBeg), tokEnd(tokEnd), pos(pos) {}

std::shared_ptr<ast::Type> Parser::type() { return type(tokBeg, tokEnd); }

std::shared_ptr<ast::Expression> Parser::expression() {
  return expression(tokBeg, tokEnd);
}

std::shared_ptr<ast::ASTRoot> Parser::root() { return root(tokBeg, tokEnd); }

bool Parser::exhausted() const { return tokBeg == tokEnd; }

/*- misc ---------------------------------------------------------------------*/

std::shared_ptr<ast::Identifier> Parser::identifier(TokIter &iter,
                                                    TokIter end) {
  if (GetTokenID(end)(iter) != TokenID::Identifier)
    return nullptr;
  auto pos = iter->position();
  return makeNode<ast::Identifier>(pos.first, pos.second,
                                   (iter++)->val<std::string>());
}

std::shared_ptr<ast::BuiltinType> Parser::builtinType(TokIter &iter,
                                                      TokIter end) {
  auto id = GetTokenID(end)(iter);
  if (id != TokenID::Bool && id != TokenID::Int && id != TokenID::String) {
    return nullptr;
  }

  Position posBeg, posEnd;
  std::tie(posBeg, posEnd) = iter->position();
  auto mk = [this, &posBeg, &posEnd](ast::BuiltinType::Type ty) {
    return makeNode<ast::BuiltinType>(posBeg, posEnd, ty);
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
    break;
  }
  assert(false);
}

std::shared_ptr<ast::NonarrayType> Parser::nonarrayType(TokIter &iter,
                                                        TokIter end) {
  if (auto res = builtinType(iter, end))
    return res;
  if (auto id = identifier(iter, end))
    return makeNode<ast::UserDefinedType>(pos[id->getID()], id);
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
    res = makeNode<ast::ArrayType>(beg->position().first,
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
    return makeNode<ast::NullLitExpr>(posBeg, posEnd);
  case TokenID::BoolLit:
    return makeNode<ast::BoolLitExpr>(posBeg, posEnd, cur->val<bool>());
  case TokenID::IntLit:
    return makeNode<ast::IntLitExpr>(posBeg, posEnd, cur->val<Integer>());
  case TokenID::StringLit:
    return makeNode<ast::StringLitExpr>(posBeg, posEnd,
                                        cur->val<std::string>());
  default:
    assert(false);
  }
  assert(false);
}

std::shared_ptr<ast::IdentifierExpr> Parser::identifierExpr(TokIter &iter,
                                                            TokIter end) {
  if (auto res = identifier(iter, end))
    return makeNode<ast::IdentifierExpr>(pos[res->getID()], res);
  return nullptr;
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

  if (id(iter) == TokenID::LeftParen) {
    if (id(++iter) != TokenID::RightParen)
      throw SyntaxError(begPos, iter->position().second);
    return makeNode<ast::NewExpr>(begPos, (iter++)->position().second, baseType,
                                  providedDims);
  }

  bool noDimNum = false;
  while (id(iter) == TokenID::LeftBracket) {
    ++iter;
    if (id(iter) == TokenID::RightBracket) {
      noDimNum = true;
      ++iter;
      baseType =
          makeNode<ast::ArrayType>(begPos, iter->position().second, baseType);
      continue;
    }
    if (noDimNum)
      throw SyntaxError(begPos, iter->position().second);
    auto expr = expression(iter, end);
    if (!expr)
      throw SyntaxError(begPos, iter->position().second);
    providedDims.emplace_back(expr);
    baseType =
        makeNode<ast::ArrayType>(begPos, iter->position().second, baseType);
    if (id(iter++) != TokenID::RightBracket)
      throw SyntaxError(begPos, iter->position().second);
  }
  return makeNode<ast::NewExpr>(begPos, iter->position().second, baseType,
                                std::move(providedDims));
}

std::shared_ptr<ast::Expression> Parser::primaryExpr(TokIter &iter,
                                                     TokIter end) {
  auto id = GetTokenID(end);
  if (auto identExpr = identifierExpr(iter, end)) {
    if (id(iter) != TokenID::LeftParen)
      return identExpr;
    ++iter;
    auto ident = identExpr->identifier;
    auto args = exprList(iter, end);
    assert(id(iter) == TokenID::RightParen);
    ++iter;
    return makeNode<ast::FuncCallExpr>(
        pos[ident->getID()], std::shared_ptr<ast::Expression>(nullptr),
        std::move(ident), std::move(args));
  }
  if (auto res = literalExpr(iter, end))
    return res;
  if (auto res = newExpr(iter, end))
    return res;

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
      res = makeNode<ast::BinaryExpr>(begPos, iter->position().second,
                                      ast::BinaryExpr::OpType::Subscript, res,
                                      expr);
      ++iter;
    } else if (id(iter) == TokenID::Dot) {
      ++iter;
      bool paren = id(iter) == TokenID::LeftParen;
      if (paren)
        ++iter;
      auto identExpr = identifierExpr(iter, end);
      if (!identExpr)
        throwError();
      if (paren) {
        if (id(iter) != TokenID::RightParen)
          throwError();
        ++iter;
      }
      if (id(iter) != TokenID::LeftParen) {
        res =
            makeNode<ast::BinaryExpr>(begPos, pos[identExpr->getID()].second,
                                      ast::BinaryExpr::Member, res, identExpr);
        continue;
      }
      // member function call
      ++iter;
      auto ident = identExpr->identifier;
      auto args = exprList(iter, end);
      assert(id(iter) == TokenID::RightParen);
      res = makeNode<ast::FuncCallExpr>(begPos, (iter++)->position().second,
                                        res, std::move(ident), std::move(args));
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
    res = makeNode<ast::UnaryExpr>(begPos, iter->position().second,
                                   ast::UnaryExpr::PostInc, res);
    ++iter;
  } else if (id == TokenID::MinusMinus) {
    res = makeNode<ast::UnaryExpr>(begPos, iter->position().second,
                                   ast::UnaryExpr::PostDec, res);
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

  std::vector<ast::UnaryExpr::OpType> prefix;
  bool flag = true;
  while (flag) {
    switch (id(iter++)) {
    case TokenID::PlusPlus:
      prefix.emplace_back(ast::UnaryExpr::PreInc);
      break;
    case TokenID::MinusMinus:
      prefix.emplace_back(ast::UnaryExpr::PreDec);
      break;
    case TokenID::Plus:
      break;
    case TokenID::Minus:
      prefix.emplace_back(ast::UnaryExpr::Neg);
      break;
    case TokenID::LogicalNot:
      prefix.emplace_back(ast::UnaryExpr::LogicalNot);
      break;
    case TokenID::BitNot:
      prefix.emplace_back(ast::UnaryExpr::BitNot);
      break;
    default:
      --iter;
      flag = false;
      break;
    }
  }

  auto res = suffixIncDec(iter, end);
  if (!res) {
    if (prefix.empty())
      return nullptr;
    throw SyntaxError(begPos, iter->position().second);
  }

  for (auto riter = prefix.rbegin(); riter != prefix.rend(); ++riter)
    res =
        makeNode<ast::UnaryExpr>(begPos, pos[res->getID()].second, *riter, res);

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
  if (!type(iter, end) || !identifier(iter, end) ||
      id(iter++) == TokenID::LeftParen) {
    iter = beg;
    return nullptr;
  }
  iter = beg;

  auto varType = type(iter, end);
  assert(varType);
  auto ident = identifier(iter, end);
  assert(ident);
  if (id(iter) == TokenID::Semicolon)
    return makeNode<ast::VarDeclStmt>(begPos, (iter++)->position().second,
                                      varType, ident, nullptr);

  if (id(iter) != TokenID::Assign)
    throwError();
  ++iter;
  auto expr = expression(iter, end);
  if (!expr)
    throwError();
  if (id(iter) != TokenID::Semicolon)
    throwError();
  return makeNode<ast::VarDeclStmt>(begPos, (iter++)->position().second,
                                    varType, ident, expr);
}

std::shared_ptr<ast::ExprStmt> Parser::exprStmt(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto expr = expression(iter, end);
  if (!expr)
    return nullptr;
  if (GetTokenID(end)(iter) != TokenID::Semicolon)
    throw SyntaxError(begPos, iter->position().second);
  return makeNode<ast::ExprStmt>(begPos, (iter++)->position().second, expr);
}

std::shared_ptr<ast::ReturnStmt> Parser::returnStmt(TokIter &iter,
                                                    TokIter end) {
  auto begPos = iter->position().first;

  auto id = GetTokenID(end);
  if (id(iter) != TokenID::Return)
    return nullptr;
  ++iter;
  if (id(iter) == TokenID::Semicolon)
    return makeNode<ast::ReturnStmt>(begPos, (iter++)->position().second,
                                     nullptr);
  auto expr = expression(iter, end);
  if (!expr || id(iter) != TokenID::Semicolon)
    throw SyntaxError(begPos, iter->position().second);
  return makeNode<ast::ReturnStmt>(begPos, (iter++)->position().second, expr);
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

  return makeNode<ast::ContinueStmt>(begPos, (iter++)->position().second);
}

std::shared_ptr<ast::BreakStmt> Parser::breakStmt(TokIter &iter, TokIter end) {
  auto begPos = iter->position().first;
  auto id = GetTokenID(end);

  if (id(iter) != TokenID::Break)
    return nullptr;
  ++iter;
  if (id(iter) != TokenID::Semicolon)
    throw SyntaxError(begPos, iter->position().second);

  return makeNode<ast::BreakStmt>(begPos, (iter++)->position().second);
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
  return makeNode<ast::CompoundStmt>(begPos, (iter++)->position().second,
                                     std::move(stmts));
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
    return makeNode<ast::IfStmt>(begPos, iter->position().second, condition,
                                 then, nullptr);
  auto else_ = statement(++iter, end);
  if (!else_)
    throw SyntaxError(begPos, iter->position().second);
  return makeNode<ast::IfStmt>(begPos, pos[else_->getID()].second, condition,
                               then, else_);
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
  return makeNode<ast::WhileStmt>(begPos, pos[body->getID()].second, condition,
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
  return makeNode<ast::ForStmt>(begPos, pos[stmt->getID()].second, init,
                                condition, update, stmt);
}

std::shared_ptr<ast::EmptyStmt> Parser::emptyStmt(TokIter &iter, TokIter end) {
  auto id = GetTokenID(end)(iter);
  if (id != TokenID::Semicolon)
    return nullptr;
  auto begPos = iter->position().first;
  return makeNode<ast::EmptyStmt>(begPos, (iter++)->position().second);
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
    return makeNode<ast::VarDecl>(pos[res->getID()], res);
  return nullptr;
}

std::shared_ptr<ast::FuncDecl> Parser::funcDecl(TokIter &iter, TokIter end) {
  auto beg = iter;
  auto id = GetTokenID(end);
  auto throwError = [beg, &iter] {
    throw SyntaxError(beg->position().first, iter->position().second);
  };

  if (!(type(iter, end) || id(iter++) == TokenID::Void) ||
      !identifier(iter, end) || id(iter++) != TokenID::LeftParen) {
    iter = beg;
    return nullptr;
  }
  iter = beg;
  auto retType = type(iter, end);
  if (!retType) {
    assert(id(iter) == TokenID::Void);
    iter++;
  }
  auto ident = identifier(iter, end);

  std::vector<std::shared_ptr<ast::VarDeclStmt>> formalParam;
  if (id(iter + 1) != TokenID::RightParen) {
    do {
      // Now [iter] points to a Comma or a LeftParen
      ++iter;
      auto ty = type(iter, end);
      if (!ty)
        throwError();
      auto paramName = identifier(iter, end);
      if (!ident)
        throwError();
      formalParam.emplace_back(makeNode<ast::VarDeclStmt>(
          pos[ty->getID()].first, pos[paramName->getID()].second, std::move(ty),
          std::move(paramName)));
    } while (id(iter) == TokenID::Comma);
  } else {
    ++iter;
  }
  ++iter;
  auto body = compoundStmt(iter, end);
  if (!body)
    throwError();
  return makeNode<ast::FuncDecl>(beg->position().first,
                                 pos[body->getID()].second, retType, ident,
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
  auto ident = identifier(iter, end);
  if (!ident)
    throwError();
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
        iter->val<std::string>() == ident->val) {
      if (iter + 3 >= end)
        throwError();
      auto ctorPos = iter->position();
      iter += 3;
      auto body = compoundStmt(iter, end);
      if (!body)
        throwError();
      auto ctor = makeNode<ast::FuncDecl>(
          ctorPos.first, pos[body->getID()].second,
          std::shared_ptr<ast::Type>(nullptr),
          makeNode<ast::Identifier>(ctorPos.first, ctorPos.second, "_ctor_"),
          std::vector<std::shared_ptr<ast::VarDeclStmt>>(), body);
      members.emplace_back(ctor);
      continue;
    }
    break;
  }
  if (id(iter) != TokenID::RightBrace)
    throwError();
  return makeNode<ast::ClassDecl>(begPos, (iter++)->position().second, ident,
                                  std::move(members));
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
  return makeNode<ast::ASTRoot>(begPos, pos[decls.back()->getID()].second,
                                std::move(decls));
}

/*- helper -------------------------------------------------------------------*/

std::vector<std::shared_ptr<ast::Expression>> Parser::exprList(TokIter &iter,
                                                               TokIter end) {
  auto first = expression(iter, end);
  if (!first)
    return {};

  std::vector<std::shared_ptr<ast::Expression>> res;
  res.emplace_back(first);

  auto id = GetTokenID(end);
  while (id(iter) == TokenID::Comma) {
    ++iter;
    auto expr = expression(iter, end);
    if (!expr)
      throw SyntaxError(iter->position().first, iter->position().second);
    res.emplace_back(std::move(expr));
  }
  return res;
}

} // namespace mocker