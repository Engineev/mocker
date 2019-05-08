#include "preprocessor.h"

#include "ast/visitor.h"

#include <iostream>

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

namespace mocker {
namespace {
class FindPureFunctions : public ast::ConstVisitor {
public:
  explicit FindPureFunctions(std::unordered_set<std::string> &pure)
      : pure(pure) {}

  void operator()(const ast::IdentifierExpr &node) const override {
    if (node.identifier->val.at(0) == '@')
      is = false;
  }

  void operator()(const ast::UnaryExpr &node) const override {
    visit(node.operand);
  }

  void operator()(const ast::BinaryExpr &node) const override {
    if (node.op == ast::BinaryExpr::Subscript ||
        node.op == ast::BinaryExpr::Member) {
      is = false;
    }
    if (node.op == ast::BinaryExpr::Assign)
      visit(node.lhs);
    visit(node.rhs);
  }

  void operator()(const ast::FuncCallExpr &node) const override { is = false; }

  void operator()(const ast::NewExpr &node) const override {
    for (auto &dim : node.providedDims)
      if (dim)
        visit(dim);
  }

  void operator()(const ast::VarDeclStmt &node) const override {
    if (node.initExpr)
      visit(node.initExpr);
  }

  void operator()(const ast::ExprStmt &node) const override {
    visit(node.expr);
  }

  void operator()(const ast::ReturnStmt &node) const override {
    if (node.expr)
      visit(node.expr);
  }

  void operator()(const ast::ContinueStmt &node) const override {}

  void operator()(const ast::BreakStmt &node) const override {}

  void operator()(const ast::CompoundStmt &node) const override {
    for (auto &stmt : node.stmts)
      visit(stmt);
  }

  void operator()(const ast::IfStmt &node) const override {
    visit(node.condition);
    visit(node.then);
    if (node.else_)
      visit(node.else_);
  }

  void operator()(const ast::WhileStmt &node) const override {
    if (node.condition)
      visit(node.condition);
    visit(node.body);
  }

  void operator()(const ast::ForStmt &node) const override {
    if (node.init)
      visit(node.init);
    if (node.condition)
      visit(node.condition);
    if (node.update)
      visit(node.update);
    visit(node.body);
  }

  void operator()(const ast::EmptyStmt &node) const override {}

  void operator()(const ast::VarDecl &node) const override { assert(false); }

  void operator()(const ast::FuncDecl &node) const override {
    visit(node.body);
  }

  void operator()(const ast::ClassDecl &node) const override { assert(false); }

  void operator()(const ast::ASTRoot &node) const override {
    for (auto &decl : node.decls) {
      auto func = std::dynamic_pointer_cast<ast::FuncDecl>(decl);
      if (!func)
        continue;
      is = true;
      visit(func);
      if (is) {
        pure.emplace(func->identifier->val);
      }
    }
  }

private:
  void visit(const std::shared_ptr<ast::ASTNode> &node) const {
    node->accept(*this);
  }

private:
  std::unordered_set<std::string> &pure;
  mutable bool is = true;
};
} // namespace

std::unordered_set<std::string>
findPureFunctions(const std::shared_ptr<ast::ASTRoot> &ast) {
  std::unordered_set<std::string> res;
  ast->accept(FindPureFunctions{res});
  return res;
}

} // namespace mocker

namespace mocker {
namespace {

class FindUnusedVariables : public ast::ConstVisitor {
public:
  FindUnusedVariables(
      std::unordered_set<std::string> &res,
      const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>>
          &exprType)
      : res(res), exprType(exprType) {}

  void operator()(const ast::Identifier &node) const override { assert(false); }

  void operator()(const ast::IdentifierExpr &node) const override {
    all.emplace(node.identifier->val);
    used.emplace(node.identifier->val);
  }

  void operator()(const ast::UnaryExpr &node) const override {
    visit(node.operand);
  }

  void operator()(const ast::BinaryExpr &node) const override {
    visit(node.rhs);
    if (lhsAssignment && node.op == ast::BinaryExpr::Subscript &&
        std::dynamic_pointer_cast<ast::IdentifierExpr>(node.lhs))
      return;
    if (node.op == ast::BinaryExpr::Assign &&
        std::dynamic_pointer_cast<ast::BuiltinType>(
            exprType.at(node.rhs->getID())))
      lhsAssignment = true;
    visit(node.lhs);
    lhsAssignment = false;
  }

  void operator()(const ast::FuncCallExpr &node) const override {
    for (auto &arg : node.args)
      visit(arg);
  }

  void operator()(const ast::NewExpr &node) const override {
    for (auto &dim : node.providedDims)
      if (dim)
        visit(dim);
  }

  void operator()(const ast::VarDeclStmt &node) const override {
    all.emplace(node.identifier->val);
    if (node.initExpr)
      visit(node.initExpr);
  }

  void operator()(const ast::ExprStmt &node) const override {
    visit(node.expr);
  }

  void operator()(const ast::ReturnStmt &node) const override {
    if (node.expr)
      visit(node.expr);
  }

  void operator()(const ast::ContinueStmt &node) const override {}

  void operator()(const ast::BreakStmt &node) const override {}

  void operator()(const ast::CompoundStmt &node) const override {
    for (auto &stmt : node.stmts)
      visit(stmt);
  }

  void operator()(const ast::IfStmt &node) const override {
    visit(node.condition);
    visit(node.then);
    if (node.else_)
      visit(node.else_);
  }

  void operator()(const ast::WhileStmt &node) const override {
    if (node.condition)
      visit(node.condition);
    visit(node.body);
  }

  void operator()(const ast::ForStmt &node) const override {
    if (node.init)
      visit(node.init);
    if (node.condition)
      visit(node.condition);
    if (node.update)
      visit(node.update);
    visit(node.body);
  }

  void operator()(const ast::EmptyStmt &node) const override {}

  void operator()(const ast::VarDecl &node) const override { assert(false); }

  void operator()(const ast::FuncDecl &node) const override {
    visit(node.body);
    res.clear();
    for (auto &var : all) {
      if (used.find(var) == used.end())
        res.emplace(var);
    }
  }

  void operator()(const ast::ClassDecl &node) const override { assert(false); }

  void operator()(const ast::ASTRoot &node) const override { assert(false); }

private:
  void visit(const std::shared_ptr<ast::ASTNode> &node) const {
    node->accept(*this);
  }

private:
  mutable std::unordered_set<std::string> used;
  mutable std::unordered_set<std::string> all;
  std::unordered_set<std::string> &res;
  const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>> &exprType;
  mutable bool lhsAssignment = false;
};

} // namespace

std::unordered_set<std::string> findUnusedVariables(
    const std::shared_ptr<ast::FuncDecl> &func,
    const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>>
        &exprType) {
  std::unordered_set<std::string> res;
  func->accept(FindUnusedVariables{res, exprType});
  std::cerr << "\nUnused variables: ";
  for (auto &var : res) {
    std::cerr << var << ", ";
  }
  std::cerr << std::endl;
  return res;
}

} // namespace mocker

namespace mocker {
namespace {

class FindRedundantNodes : public ast::ConstVisitor {
public:
  FindRedundantNodes(
      std::unordered_set<ast::NodeID> &res,
      const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>>
          &exprType,
      const std::unordered_set<std::string> &pureFunc)
      : res(res), exprType(exprType), pureFunc(pureFunc) {}

  void operator()(const ast::IdentifierExpr &node) const override {
    identifier = node.identifier->val;
  }

  void operator()(const ast::FuncCallExpr &node) const override {
    if (pureFunc.find(node.identifier->val) != pureFunc.end()) {
      hasSideEffect = true;
      return;
    }
    for (auto &arg : node.args)
      visit(arg);
  }

  void operator()(const ast::NewExpr &node) const override {
    for (auto &dim : node.providedDims) {
      if (dim)
        visit(dim);
    }
  }

  void operator()(const ast::UnaryExpr &node) const override {
    if (node.op == ast::UnaryExpr::PostInc ||
        node.op == ast::UnaryExpr::PostDec ||
        node.op == ast::UnaryExpr::PreInc || node.op == ast::UnaryExpr::PreDec)
      hasSideEffect = true;
  }

  void operator()(const ast::BinaryExpr &node) const override {
    visit(node.rhs);
    if (node.op == ast::BinaryExpr::Assign) {
      hasSideEffect = false;
      auto intType = std::dynamic_pointer_cast<ast::BuiltinType>(
          exprType.at(node.rhs->getID()));
      if (!intType) {
        hasSideEffect = true;
        return;
      }
      identifier = "";
      visit(node.lhs);
      if (!hasSideEffect && unusedVars.find(identifier) != unusedVars.end()) {
        res.emplace(node.getID());
      }
      return;
    }
    visit(node.lhs);
  }

  void operator()(const ast::ExprStmt &node) const override {
    visit(node.expr);
  }

  void operator()(const ast::CompoundStmt &node) const override {
    for (auto &stmt : node.stmts)
      visit(stmt);
  }

  void operator()(const ast::IfStmt &node) const override {
    visit(node.then);
    if (node.else_)
      visit(node.else_);
  }

  void operator()(const ast::WhileStmt &node) const override {
    visit(node.body);
  }

  void operator()(const ast::ForStmt &node) const override { visit(node.body); }

  void operator()(const ast::EmptyStmt &node) const override {}

  void operator()(const ast::VarDecl &node) const override { assert(false); }

  void operator()(const ast::FuncDecl &node) const override {
    unusedVars = findUnusedVariables(
        std::static_pointer_cast<ast::FuncDecl>(
            (const_cast<ast::FuncDecl &>(node)).shared_from_this()),
        exprType);
    visit(node.body);
  }

  void operator()(const ast::ClassDecl &node) const override { assert(false); }

  void operator()(const ast::ASTRoot &node) const override {
    for (auto &decl : node.decls) {
      auto func = std::dynamic_pointer_cast<ast::FuncDecl>(decl);
      if (!func)
        continue;
      visit(func);
    }
  }

private:
  void visit(const std::shared_ptr<ast::ASTNode> &node) const {
    node->accept(*this);
  }

private:
  mutable bool hasSideEffect = false;
  mutable std::unordered_set<std::string> unusedVars;
  std::unordered_set<ast::NodeID> &res;
  const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>> &exprType;
  const std::unordered_set<std::string> &pureFunc;
  mutable std::string identifier;
};

} // namespace

std::unordered_set<ast::NodeID> findRedundantNodes(
    const std::shared_ptr<ast::ASTNode> &ast,
    const std::unordered_map<ast::NodeID, std::shared_ptr<ast::Type>> &exprType,
    const std::unordered_set<std::string> &pureFunc) {
  std::unordered_set<ast::NodeID> res;
  ast->accept(FindRedundantNodes{res, exprType, pureFunc});
  return res;
}

} // namespace mocker