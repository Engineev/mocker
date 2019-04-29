#ifndef MOCKER_BUILD_H
#define MOCKER_BUILD_H

#include "ast/ast_node.h"
#include "ir/module.h"
#include "semantic/semantic_context.h"

namespace mocker {

ir::Module buildIR(const std::shared_ptr<ast::ASTRoot> &ast,
                   const SemanticContext &semanticCtx);

} // namespace mocker

#endif // MOCKER_BUILD_H
