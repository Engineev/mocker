#include "parser.h"

#include <cassert>
#include <functional>
#include <unordered_map>

#include "ast/ast_node.h"

namespace mocker {

Parser::Parser(TokIter tokBeg, TokIter tokEnd)
    : tokBeg(tokBeg), tokEnd(tokEnd) {}

std::shared_ptr<ast::Type> Parser::type() { return type(tokBeg, tokEnd); }

std::shared_ptr<ast::Expression> Parser::expression() {
  return expression(tokBeg, tokEnd);
}

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
    if (id(iter) != TokenID::RightBracket)
      throw SyntaxError(beg->position().first, iter->position().second);
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
              const std::unordered_map<TokenID, ast::BinaryExpr::OpType> &mp) {
  auto begPos = iter->position().first;
  auto res = operand(iter, end);
  if (!res)
    return nullptr;

  auto id = GetTokenID(end);
  while (mp.find(id(iter)) != mp.end()) {
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

std::shared_ptr<ast::Expression> Parser::expression(TokIter &iter, TokIter end) {
  return assignmentExpr(iter, end);
}

} // namespace mocker