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

  bool isLeftValue(ast::NodeID v) const {
    return leftValue.find(v) != leftValue.end();
  }

  SymTbl syms;
  std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>> exprType;
  std::unordered_set<ast::NodeID> leftValue;
  std::unordered_map<ast::NodeID, ScopeID> scopeIntroduced;
};

} // namespace mocker

#endif // MOCKER_SEMANTIC_CONTEXT_H
