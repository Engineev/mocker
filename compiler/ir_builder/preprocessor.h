#ifndef MOCKER_PREPROCESSOR_H
#define MOCKER_PREPROCESSOR_H

#include <unordered_map>

#include "ast/ast_node.h"
#include "semantic/sym_tbl.h"

namespace mocker {

void renameASTIdentifiers(
    const std::shared_ptr<ast::ASTRoot> &ast,
    const std::unordered_map<ast::NodeID, ScopeID> &scopeResiding,
    const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Declaration>>
        &associatedDecl,
    const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>>
        &exprType);
}

#endif // MOCKER_PREPROCESSOR_H
