#ifndef MOCKER_PREPROCESSOR_H
#define MOCKER_PREPROCESSOR_H

#include <string>
#include <unordered_map>
#include <unordered_set>

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

std::unordered_set<std::string>
findPureFunctions(const std::shared_ptr<ast::ASTRoot> &ast);

std::unordered_set<std::string>
findUnusedVariables(const std::shared_ptr<ast::FuncDecl> &func);

std::unordered_set<ast::NodeID> findRedundantNodes(
    const std::shared_ptr<ast::ASTNode> &ast,
    const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>> &exprType,
    const std::unordered_set<std::string> &pureFunc);

} // namespace mocker

#endif // MOCKER_PREPROCESSOR_H
