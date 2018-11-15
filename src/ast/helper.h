#ifndef MOCKER_HELPER_H
#define MOCKER_HELPER_H

#include <memory>
#include <string>
#include <utility>

#include "ast_node.h"

namespace mocker {
namespace ast {

// Get the inner most type from a arrayType.
// If [type] is not an arrayType, return directly.
inline std::shared_ptr<NonarrayType>
stripType(const std::shared_ptr<Type> &type) {
  if (auto res = std::dynamic_pointer_cast<NonarrayType>(type))
    return res;
  auto baseType = std::static_pointer_cast<ArrayType>(type)->baseType;
  return stripType(baseType);
}

// Get the identifier of a type
inline std::string getTypeIdentifier(const std::shared_ptr<Type> &type) {
  if (auto ptr = std::dynamic_pointer_cast<ArrayType>(type))
    return getTypeIdentifier(ptr->baseType) + "[]";

  if (auto ptr = std::dynamic_pointer_cast<UserDefinedType>(type))
    return ptr->name->val;
  auto ptr = std::static_pointer_cast<BuiltinType>(type);
  assert(ptr);
  switch (ptr->type) {
  case BuiltinType::Bool:
    return "bool";
  case BuiltinType::Int:
    return "int";
  case BuiltinType::String:
    return "string";
  case BuiltinType::Null:
    return "_null";
  }
  assert(false);
}

} // namespace ast
} // namespace mocker

namespace mocker {
namespace mk_ast {

inline Position pos() { return Position(); }

inline std::shared_ptr<ast::Identifier> ident(std::string name) {
  return std::make_shared<ast::Identifier>(pos(), pos(), std::move(name));
}

inline std::shared_ptr<ast::BuiltinType> tyInt() {
  static auto res =
      std::make_shared<ast::BuiltinType>(pos(), pos(), ast::BuiltinType::Int);
  return res;
}

inline std::shared_ptr<ast::BuiltinType> tyString() {
  static auto res = std::make_shared<ast::BuiltinType>(
      pos(), pos(), ast::BuiltinType::String);
  return res;
}

inline std::shared_ptr<ast::BuiltinType> tyBool() {
  static auto res =
      std::make_shared<ast::BuiltinType>(pos(), pos(), ast::BuiltinType::Bool);
  return res;
}

inline std::shared_ptr<ast::BuiltinType> tyNull() {
  static auto res =
      std::make_shared<ast::BuiltinType>(pos(), pos(), ast::BuiltinType::Null);
  return res;
}

using FormalParam =
    std::pair<std::shared_ptr<ast::Type>, std::shared_ptr<ast::Identifier>>;

inline FormalParam param(const std::shared_ptr<ast::Type> &ty) {
  return std::make_pair(ty, ident(""));
}

inline std::vector<FormalParam> emptyParams() { return {}; }

inline std::shared_ptr<ast::CompoundStmt> body() { return nullptr; }

} // namespace mk_ast
} // namespace mocker

#endif // MOCKER_HELPER_H
