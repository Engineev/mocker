#include "preprocessor.h"

#include "ast/visitor.h"

namespace mocker {

class RenameASTDeclarations : public ast::Visitor {
public:
  explicit RenameASTDeclarations(
      const std::unordered_map<ast::NodeID, ScopeID> &scopeResiding)
      : scopeResiding(scopeResiding) {}

  void operator()(ast::VarDeclStmt &node) const override {
    renameLocalVar(node);
  }

  void operator()(ast::CompoundStmt &node) const override {
    for (auto &stmt : node.stmts)
      visit(stmt);
  }

  void operator()(ast::IfStmt &node) const override {
    visit(node.then);
    if (node.else_)
      visit(node.else_);
  }

  void operator()(ast::WhileStmt &node) const override { visit(node.body); }

  void operator()(ast::ForStmt &node) const override { visit(node.body); }

  void operator()(ast::VarDecl &node) const override { assert(false); }

  void operator()(ast::FuncDecl &node) const override {
    for (auto &param : node.formalParameters)
      visit(param);
    visit(node.body);
  }

  void operator()(ast::ClassDecl &node) const override {
    for (auto &member : node.members)
      if (auto p = std::dynamic_pointer_cast<ast::FuncDecl>(member))
        renameMemberFunc(*p, node.identifier->val);

    std::vector<std::shared_ptr<ast::VarDecl>> memberVars;
    for (auto &member : node.members)
      if (auto p = std::dynamic_pointer_cast<ast::VarDecl>(member))
        renameMemberVar(*p, node.identifier->val);

    for (auto &member : node.members) {
      if (std::dynamic_pointer_cast<ast::VarDecl>(member))
        continue;
      visit(member);
    }
  }

  void operator()(ast::ASTRoot &node) const override {
    std::vector<std::shared_ptr<ast::VarDecl>> globalVars;
    for (auto &decl : node.decls)
      if (auto p = std::dynamic_pointer_cast<ast::VarDecl>(decl))
        renameGlobalVar(*p);

    for (auto &decl : node.decls) {
      if (std::dynamic_pointer_cast<ast::VarDecl>(decl))
        continue;
      visit(decl);
    }
  }

private:
  void visit(const std::shared_ptr<ast::ASTNode> &p) const { p->accept(*this); }

  void renameGlobalVar(ast::VarDecl &node) const {
    auto oldIdent = node.decl->identifier->val;
    node.decl->identifier->val = "@" + oldIdent;
  }

  void renameMemberVar(ast::VarDecl &node, const std::string &className) const {
    auto oldIdent = node.decl->identifier->val;
    node.decl->identifier->val = "#" + className + "#" + oldIdent;
  }

  void renameLocalVar(ast::VarDeclStmt &node) const {
    auto scopeInfo = scopeResiding.at(node.getID()).fmt();
    assert(scopeInfo.find('_') == std::string::npos);
    node.identifier->val = scopeInfo + '_' + node.identifier->val;
  }

  void renameMemberFunc(ast::FuncDecl &node,
                        const std::string &className) const {
    auto oldIdent = node.identifier->val;
    node.identifier->val = "#" + className + "#" + oldIdent;
  }

  const std::unordered_map<ast::NodeID, ScopeID> &scopeResiding;
};

class RenameASTIdentifiers : public ast::Visitor {
public:
  RenameASTIdentifiers(
      const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Declaration>>
          &associatedDecl,
      const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>>
          &exprType)
      : associatedDecl(associatedDecl), exprType(exprType) {}

  void operator()(ast::Identifier &node) const override { assert(false); }

  void operator()(ast::IdentifierExpr &node) const override {
    if (node.identifier->val == "this")
      return;
    auto decl = std::dynamic_pointer_cast<ast::VarDecl>(
        associatedDecl.at(node.identifier->getID()));
    assert(decl);
    node.identifier->val = decl->decl->identifier->val;
  }

  void operator()(ast::UnaryExpr &node) const override { visit(node.operand); }

  void operator()(ast::BinaryExpr &node) const override {
    visit(node.lhs);
    if (node.op != ast::BinaryExpr::Member)
      visit(node.rhs);
  }

  void operator()(ast::FuncCallExpr &node) const override {
    for (auto &arg : node.args)
      visit(arg);
    if (!node.instance) // free functions
      return;
    visit(node.instance);

    auto &ident = node.identifier->val;

    if (auto p = std::dynamic_pointer_cast<ast::ArrayType>(
            exprType.at(node.instance->getID()))) {
      assert(ident == "size");
      ident = "#_array_#size";
      return;
    }
    if (auto p = std::dynamic_pointer_cast<ast::BuiltinType>(
            exprType.at(node.instance->getID()))) {
      assert(p->type == ast::BuiltinType::String);
      ident = "#string#" + ident;
      return;
    }

    auto funcDecl = std::static_pointer_cast<ast::FuncDecl>(
        associatedDecl.at(node.identifier->getID()));
    assert(funcDecl);
    ident = funcDecl->identifier->val;
  }

  void operator()(ast::NewExpr &node) const override {
    for (auto &exp : node.providedDims)
      visit(exp);
  }

  void operator()(ast::VarDeclStmt &node) const override {
    if (node.initExpr)
      visit(node.initExpr);
  }

  void operator()(ast::ExprStmt &node) const override { visit(node.expr); }

  void operator()(ast::ReturnStmt &node) const override {
    if (node.expr)
      visit(node.expr);
  }

  void operator()(ast::CompoundStmt &node) const override {
    for (auto &stmt : node.stmts)
      visit(stmt);
  }

  void operator()(ast::IfStmt &node) const override {
    visit(node.condition);
    visit(node.then);
    if (node.else_)
      visit(node.else_);
  }

  void operator()(ast::WhileStmt &node) const override {
    visit(node.condition);
    visit(node.body);
  }

  void operator()(ast::ForStmt &node) const override {
    if (node.init)
      visit(node.init);
    if (node.condition)
      visit(node.condition);
    if (node.update)
      visit(node.update);
    visit(node.body);
  }

  void operator()(ast::VarDecl &node) const override { visit(node.decl); }

  void operator()(ast::FuncDecl &node) const override { visit(node.body); }

  void operator()(ast::ClassDecl &node) const override {
    for (auto &member : node.members)
      visit(member);
  }

  void operator()(ast::ASTRoot &node) const override {
    for (auto &decl : node.decls)
      visit(decl);
  }

private:
  void visit(const std::shared_ptr<ast::ASTNode> &p) const { p->accept(*this); }

private:
  const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Declaration>>
      &associatedDecl;
  const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>> &exprType;
};

void renameASTIdentifiers(
    const std::shared_ptr<ast::ASTRoot> &ast,
    const std::unordered_map<ast::NodeID, ScopeID> &scopeResiding,
    const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Declaration>>
        &associatedDecl,
    const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>>
        &exprType) {
  ast->accept(RenameASTDeclarations(scopeResiding));
  ast->accept(RenameASTIdentifiers(associatedDecl, exprType));
}

} // namespace mocker