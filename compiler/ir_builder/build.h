#ifndef MOCKER_BUILD_H
#define MOCKER_BUILD_H

#include "ast/ast_node.h"
#include "builder.h"
#include "builder_context.h"
#include "ir/module.h"
#include "preprocessor.h"
#include "semantic/semantic_context.h"

namespace mocker {

inline ir::Module buildIR(const std::shared_ptr<ast::ASTRoot> &ast,
                          const SemanticContext &semanticCtx) {
  renameASTIdentifiers(ast, semanticCtx.scopeResiding,
                       semanticCtx.associatedDecl, semanticCtx.exprType);

  ir::BuilderContext IRCtx(semanticCtx.exprType);
  ast->accept(ir::Builder(IRCtx));
  return std::move(IRCtx.getResult());
}

} // namespace mocker

#endif // MOCKER_BUILD_H
