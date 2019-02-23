#ifndef MOCKER_SEMANTIC_CONTEXT_H
#define MOCKER_SEMANTIC_CONTEXT_H

#include <cassert>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "ast/ast_node.h"
#include "sym_tbl.h"

namespace mocker {

struct SemanticContext {
  SemanticContext() = default;
  SemanticContext(SemanticContext &&) noexcept = default;
  SemanticContext &operator=(SemanticContext &&) noexcept = default;

  bool isLeftValue(ast::NodeID v) const {
    return leftValue.find(v) != leftValue.end();
  }

  enum class State { Unused, Completed, Dirty } state = State::Unused;
  SymTbl syms;
  std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>> exprType;
  std::unordered_set<ast::NodeID> leftValue;
  std::unordered_map<ast::NodeID, ScopeID> scopeIntroduced;

  std::unordered_map<ast::NodeID, ScopeID> scopeResiding;
  // Map the declarations (including VarDeclStmt's) to the scope they reside

  std::unordered_map<ast::NodeID, std::shared_ptr<ast::Declaration>>
      associatedDecl;
  // Map the variable identifiers in IdentifierExpr and VarDeclStmt and the
  // function identifiers in FuncCallExpr to their declarations.
};

} // namespace mocker

#endif // MOCKER_SEMANTIC_CONTEXT_H
