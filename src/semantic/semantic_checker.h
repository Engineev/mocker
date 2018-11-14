#ifndef MOCKER_SEMANTIC_CHECKER_H
#define MOCKER_SEMANTIC_CHECKER_H

#include <memory>
#include <vector>
#include <utility>

#include "ast/fwd.h"
#include "ast/scope_id.h"
#include "common/position.h"

namespace mocker {
// fwd
namespace ast {
class SymTbl;
}

class SemanticChecker {
public:
  explicit SemanticChecker(std::shared_ptr<ast::ASTRoot> ast);

  // Annotate the AST and build the symbol table.
  // Throw if a semantic error occurs.
  ast::SymTbl annotate();

private:
  void initSymTbl(ast::SymTbl &syms);


  // Collect symbols for forward reference. No type check will be performed but
  // duplicated symbols, if exist, will be detected.
  void
  collectSymbols(const std::vector<std::shared_ptr<ast::Declaration>> &decls,
                 const ast::ScopeID &scope, ast::SymTbl &syms);


  std::shared_ptr<ast::ASTRoot> ast;

private:
  // helpers
  using FormalParem = std::pair<std::shared_ptr<ast::Type>, std::shared_ptr<ast::Identifier>>;
};

} // namespace mocker

#endif // MOCKER_SEMANTIC_CHECKER_H
