#ifndef MOCKER_SEMANTIC_CHECKER_H
#define MOCKER_SEMANTIC_CHECKER_H

#include <memory>
#include <utility>
#include <vector>

#include "ast/fwd.h"
#include "common/position.h"
#include "semantic_context.h"
#include "sym_tbl.h"

namespace mocker {

class SemanticChecker {
public:
  explicit SemanticChecker(std::shared_ptr<ast::ASTRoot> ast,
                           std::unordered_map<ast::NodeID, PosPair> &pos);

  // Throw if a semantic error occurs.
  void check();

  void renameIdentifiers();

  const auto &getExprType() const {
    if (ctx.state != SemanticContext::State::Completed)
      std::terminate();
    return ctx.exprType;
  }

private:
  void initSymTbl(SymTbl &syms);

  // Collect symbols for forward reference. No type check will be performed but
  // duplicated symbols, if exist, will be detected.
  void
  collectSymbols(const std::vector<std::shared_ptr<ast::Declaration>> &decls,
                 const ScopeID &scope, SymTbl &syms);

  SemanticContext ctx;
  std::shared_ptr<ast::ASTRoot> ast;
  std::unordered_map<ast::NodeID, PosPair> &pos;
};

} // namespace mocker

#endif // MOCKER_SEMANTIC_CHECKER_H
