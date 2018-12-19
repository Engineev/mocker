#ifndef MOCKER_SEMANTIC_CONTEXT_H
#define MOCKER_SEMANTIC_CONTEXT_H

#include <unordered_map>
#include <unordered_set>
#include <cstddef>
#include <cstddef>
#include <cassert>
#include <memory>

#include "sym_tbl.h"
#include "ast/ast_node.h"

namespace mocker {

struct SemanticContext {

  bool isLeftValue(std::uintptr_t v) const {
    return leftValue.find(v) != leftValue.end();
  }

  SymTbl syms;
  std::unordered_map<std::uintptr_t, std::shared_ptr<ast::Type>> exprType;
  std::unordered_set<std::uintptr_t> leftValue;
  std::unordered_map<std::uintptr_t, ScopeID> scopeIntroduced;
};

}

#endif //MOCKER_SEMANTIC_CONTEXT_H
