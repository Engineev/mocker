#include "semantic_checker.h"

#include <utility>

#include "ast/ast_node.h"
#include "ast/helper.h"
#include "ast/sym_tbl.h"
#include "ast/visitor.h"
#include "error.h"

// annotate
namespace mocker {

class Annotator : public ast::Visitor {
public:
  Annotator(ast::ScopeID scopeResiding, ast::SymTbl &syms, bool inLoop,
            bool inFunc, std::shared_ptr<ast::Type> retType)
      : scopeResiding(std::move(scopeResiding)), syms(syms), inLoop(inLoop),
        inFunc(inFunc), retType(std::move(retType)) {
    builtinBool = mk_ast::tyBool();
    builtinInt = mk_ast::tyInt();
    builtinNull = mk_ast::tyNull();
    builtinString = mk_ast::tyString();
  }

  void operator()(ast::Identifier &node) const override {
    // The semantic check of an identifier varies. Therefore it makes no sense
    // to provide this function
    assert(false);
  }

  void operator()(ast::BuiltinType &node) const override { /* empty */
  }

  void operator()(ast::UserDefinedType &node) const override {
    if (!std::dynamic_pointer_cast<ast::ClassDecl>(
            syms.lookUp(scopeResiding, node.name->val)))
      throw UnresolvableSymbol(node.posBeg, node.posEnd, node.name->val);
  }

  void operator()(ast::ArrayType &node) const override { visit(node.baseType); }

  void operator()(ast::IntLitExpr &node) const override {
    node.type = builtinInt;
  }

  void operator()(ast::BoolLitExpr &node) const override {
    node.type = builtinBool;
  }

  void operator()(ast::StringLitExpr &node) const override {
    node.type = builtinString;
  }

  void operator()(ast::NullLitExpr &node) const override {
    node.type = builtinNull;
  }

  void operator()(ast::IdentifierExpr &node) const override {
    auto decl = std::dynamic_pointer_cast<ast::VarDecl>(
        syms.lookUp(scopeResiding, node.identifier->val));
    if (!decl)
      throw UnresolvableSymbol(node.posBeg, node.posEnd, node.identifier->val);
    node.type = decl->decl->type;
    node.leftValue = node.identifier->val != "this";
  }

  void operator()(ast::UnaryExpr &node) const override {
    visit(node.operand);
    if (ast::UnaryExpr::LogicalNot == node.op) {
      if (!assignable(builtinBool, node.operand->type))
        throw IncompatibleTypes(node.posBeg, node.posEnd);
    } else if (!assignable(builtinInt, node.operand->type)) {
      throw IncompatibleTypes(node.posBeg, node.posEnd);
    }
    if (ast::UnaryExpr::PreDec == node.op ||
        ast::UnaryExpr::PreInc == node.op) {
      node.leftValue = true;
      if (!node.operand->leftValue)
        throw InvalidRightValueOperation(node.posBeg, node.posEnd);
    }

    node.type = node.operand->type;
  }

  void operator()(ast::BinaryExpr &node) const override {
    auto ce = [&node] { throw IncompatibleTypes(node.posBeg, node.posEnd); };
    auto ceRv = [&node] {
      throw InvalidRightValueOperation(node.posBeg, node.posEnd);
    };

    visit(node.lhs);
    if (node.op != ast::BinaryExpr::Member)
      visit(node.rhs);
    switch (node.op) {
    case ast::BinaryExpr::Assign:
      if (!assignable(node.lhs->type, node.rhs->type))
        ce();
      if (!node.lhs->leftValue)
        ceRv();
      node.type = node.lhs->type;
      return;
    case ast::BinaryExpr::LogicalOr:
    case ast::BinaryExpr::LogicalAnd:
      if (!assignable(builtinBool, node.lhs->type) ||
          !assignable(builtinBool, node.rhs->type))
        ce();
      node.type = builtinBool;
      return;
    case ast::BinaryExpr::Add:
      if (!assignable(node.lhs->type, node.rhs->type))
        ce();
      if (assignable(builtinInt, node.lhs->type)) {
        node.type = builtinInt;
        return;
      } else if (assignable(builtinString, node.lhs->type)) {
        node.type = builtinString;
        return;
      }
      ce();
    case ast::BinaryExpr::BitOr:
    case ast::BinaryExpr::BitAnd:
    case ast::BinaryExpr::Xor:
    case ast::BinaryExpr::Shl:
    case ast::BinaryExpr::Shr:
    case ast::BinaryExpr::Sub:
    case ast::BinaryExpr::Mul:
    case ast::BinaryExpr::Div:
    case ast::BinaryExpr::Mod:
      if (!assignable(builtinInt, node.lhs->type) ||
          !assignable(builtinInt, node.rhs->type))
        ce();
      node.type = builtinInt;
      return;
    case ast::BinaryExpr::Eq:
    case ast::BinaryExpr::Ne:
      node.type = builtinBool;
      if (!assignable(node.lhs->type, node.rhs->type))
        ce();
      if (std::dynamic_pointer_cast<ast::NullLitExpr>(node.lhs) &&
          std::dynamic_pointer_cast<ast::NullLitExpr>(node.rhs))
        return;
      if (std::dynamic_pointer_cast<ast::NullLitExpr>(node.lhs) ||
          std::dynamic_pointer_cast<ast::NullLitExpr>(node.rhs)) {
        auto ty = (bool)std::dynamic_pointer_cast<ast::NullLitExpr>(node.lhs)
                      ? node.rhs->type
                      : node.lhs->type;
        if (!std::dynamic_pointer_cast<ast::UserDefinedType>(ty) &&
            !std::dynamic_pointer_cast<ast::ArrayType>(ty) &&
            !assignable(builtinString, ty))
          ce();
        else
          return;
      }
      // return; Fall over
    case ast::BinaryExpr::Lt:
    case ast::BinaryExpr::Gt:
    case ast::BinaryExpr::Le:
    case ast::BinaryExpr::Ge:
      if (!assignable(node.lhs->type, node.rhs->type))
        ce();
      if (!assignable(builtinInt, node.lhs->type) &&
          !assignable(builtinString, node.lhs->type) &&
          !assignable(builtinBool, node.lhs->type))
        ce();
      node.type = builtinBool;
      return;
    case ast::BinaryExpr::Subscript: {
      auto arrayTy = std::dynamic_pointer_cast<ast::ArrayType>(node.lhs->type);
      if (!arrayTy)
        throw CompileError(node.lhs->posBeg, node.lhs->posEnd);
      if (!assignable(builtinInt, node.rhs->type))
        ce();
      node.type = arrayTy->baseType;
      node.leftValue = true;
      return;
    }
    case ast::BinaryExpr::Member:
      if (auto ptr =
              std::dynamic_pointer_cast<ast::UserDefinedType>(node.lhs->type)) {
        auto decl = std::dynamic_pointer_cast<ast::ClassDecl>(
            syms.lookUp(scopeResiding, ptr->name->val));
        auto identExpr =
            std::dynamic_pointer_cast<ast::IdentifierExpr>(node.rhs);
        if (!identExpr)
          ce();
        auto memberVarDecl = std::dynamic_pointer_cast<ast::VarDecl>(
            syms.lookUp(decl->scopeIntroduced, identExpr->identifier->val));
        if (!memberVarDecl)
          ce();
        node.type = memberVarDecl->decl->type;
      } else
        ce();
      node.leftValue = true;
      return;
    }
  }

  void operator()(ast::FuncCallExpr &node) const override {
    std::shared_ptr<ast::FuncDecl> funcDecl;
    bool isCtor = false;
    if (node.instance) {
      visit(node.instance);
      if (assignable(builtinString, node.instance->type)) {
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(
            syms.lookUp(syms.global(), "_string_" + node.identifier->val));
        assert(funcDecl);
      } else if (std::dynamic_pointer_cast<ast::ArrayType>(
                     node.instance->type)) {
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(
            syms.lookUp(syms.global(), "_array_" + node.identifier->val));
        assert(funcDecl);
      } else {
        auto instType = std::dynamic_pointer_cast<ast::UserDefinedType>(
            node.instance->type);
        assert(instType);
        auto classDecl = std::dynamic_pointer_cast<ast::ClassDecl>(
            syms.lookUp(syms.global(), ast::getTypeIdentifier(instType)));
        assert(classDecl);
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(
            syms.lookUp(classDecl->scopeIntroduced, node.identifier->val));
      }
    } else {
      auto decl = syms.lookUp(scopeResiding, node.identifier->val);
      if (auto ptr = std::dynamic_pointer_cast<ast::ClassDecl>(decl)) {
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(
            syms.lookUp(ptr->scopeIntroduced, "_ctor_" + ptr->identifier->val));
        isCtor = true;
        node.type = std::make_shared<ast::UserDefinedType>(
            mk_ast::pos(), mk_ast::pos(), ptr->identifier);
      } else {
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(decl);
      }
    }
    if (!funcDecl)
      throw UnresolvableSymbol(node.identifier->posBeg, node.identifier->posEnd,
                               node.identifier->val);

    if (node.args.size() != funcDecl->formalParameters.size())
      throw InvalidFunctionCall(node.posBeg, node.posEnd);
    for (std::size_t i = 0; i < node.args.size(); ++i) {
      visit(node.args[i]);
      if (!assignable(funcDecl->formalParameters[i].first, node.args[i]->type))
        throw InvalidFunctionCall(node.posBeg, node.posEnd);
    }
    if (!isCtor)
      node.type = funcDecl->retType;
  }

  void operator()(ast::NewExpr &node) const override {
    for (auto &expr : node.providedDims) {
      visit(expr);
      if (!assignable(builtinInt, expr->type))
        throw IncompatibleTypes(expr->posBeg, expr->posEnd);
    }
  }

  void operator()(ast::VarDeclStmt &node) const override {
    checkVarDecl(node);
    bool addRes = syms.addSymbol(scopeResiding, node.identifier->val,
                                 std::make_shared<ast::VarDecl>(
                                     mk_ast::pos(), mk_ast::pos(),
                                     std::static_pointer_cast<ast::VarDeclStmt>(
                                         node.shared_from_this())));

    if (!addRes)
      throw DuplicatedSymbols(node.posBeg, node.posEnd, node.identifier->val);
  }

  void operator()(ast::ExprStmt &node) const override { visit(node.expr); }

  void operator()(ast::ReturnStmt &node) const override {
    if (!inFunc)
      throw ReturnOutOfAFunction(node.posBeg, node.posEnd);
    if (!node.expr) {
      if (!retType)
        return;
      throw IncompatibleTypes(node.posBeg, node.posEnd);
    }

    visit(node.expr);
    if (!assignable(retType, node.expr->type))
      throw IncompatibleTypes(node.expr->posBeg, node.expr->posEnd);
  }

  void operator()(ast::ContinueStmt &node) const override {
    if (!inLoop)
      throw ContinueOutOfALoop(node.posBeg, node.posEnd);
  }

  void operator()(ast::BreakStmt &node) const override {
    if (!inLoop)
      throw BreakOutOfALoop(node.posBeg, node.posEnd);
  }

  void operator()(ast::CompoundStmt &node) const override {
    auto scopeIntroduced = syms.createSubscope(scopeResiding);
    for (auto &stmt : node.stmts)
      visit(stmt, scopeIntroduced);
  }

  void operator()(ast::IfStmt &node) const override {
    visit(node.condition);
    if (!assignable(builtinBool, node.condition->type))
      throw IncompatibleTypes(node.condition->posBeg, node.condition->posEnd);
    auto thenScope = syms.createSubscope(scopeResiding);
    visit(node.then, thenScope);
    if (node.else_) {
      auto elseScope = syms.createSubscope(scopeResiding);
      visit(node.else_, elseScope);
    }
  }

  void operator()(ast::WhileStmt &node) const override {
    visit(node.condition, scopeResiding);
    if (!assignable(builtinBool, node.condition->type))
      throw IncompatibleTypes(node.condition->posBeg, node.condition->posEnd);

    auto scopeIntroduced = syms.createSubscope(scopeResiding);
    visit(node.body, scopeIntroduced, true, inFunc, retType);
  }

  void operator()(ast::ForStmt &node) const override {
    if (node.init)
      visit(node.init, scopeResiding);
    if (node.condition) {
      visit(node.condition, scopeResiding);
      if (!assignable(builtinBool, node.condition->type))
        throw IncompatibleTypes(node.condition->posBeg, node.condition->posEnd);
    }
    if (node.update)
      visit(node.update, scopeResiding);

    auto scopeIntroduced = syms.createSubscope(scopeResiding);
    visit(node.body, scopeIntroduced, true, inFunc, retType);
  }

  void operator()(ast::EmptyStmt &node) const override { /* pass */
  }

  void operator()(ast::VarDecl &node) const override { visit(node.decl); }

  void operator()(ast::FuncDecl &node) const override {
    if (node.retType)
      visit(node.retType);

    auto scopeIntroduced = syms.createSubscope(scopeResiding);

    for (const auto &param : node.formalParameters) {
      auto paramDecl = std::make_shared<ast::VarDecl>(
          param.first->posBeg, param.second->posEnd,
          std::make_shared<ast::VarDeclStmt>(
              param.first->posBeg, param.second->posEnd, param.first,
              param.second, std::shared_ptr<ast::Expression>(nullptr)));
      visit(paramDecl, scopeIntroduced);
    }

    for (auto &stmt : node.body->stmts) {
      if (auto ptr = std::dynamic_pointer_cast<ast::VarDeclStmt>(stmt)) {
        visit(std::make_shared<ast::VarDecl>(stmt->posBeg, stmt->posEnd, ptr),
              scopeIntroduced, false, true, node.retType);
      } else {
        visit(stmt, scopeIntroduced, false, true, node.retType);
      }
    }
  }

  void operator()(ast::ClassDecl &node) const override {
    for (auto &member : node.members) {
      if (auto ptr = std::dynamic_pointer_cast<ast::VarDecl>(member)) {
        checkVarDecl(*ptr->decl);
        continue;
      }
      visit(member, node.scopeIntroduced);
    }
  }

  void operator()(ast::ASTRoot &node) const override {
    for (auto &decl : node.decls) {
#ifndef DISABLE_FORWARD_REFERENCE_FOR_GLOBAL_VAR
      if (auto ptr = std::dynamic_pointer_cast<ast::VarDecl>(decl)) {
        checkVarDecl(*ptr->decl);
        continue;
      }
#endif
      visit(decl, syms.global());
    }
  }

private:
  void visit(const std::shared_ptr<ast::ASTNode> &node) const {
    visit(node, scopeResiding);
  }

  void visit(const std::shared_ptr<ast::ASTNode> &node,
             ast::ScopeID residing) const {
    visit(node, std::move(residing), inLoop, inFunc, retType);
  }

  void visit(const std::shared_ptr<ast::ASTNode> &node, ast::ScopeID residing,
             bool inLoop, bool inFunc,
             std::shared_ptr<ast::Type> retType) const {
    node->accept(Annotator(std::move(residing), syms, inLoop, inFunc,
                           std::move(retType)));
  }

  bool assignable(const std::shared_ptr<ast::Type> &lhs,
                  const std::shared_ptr<ast::Type> &rhs) const {
    if (!lhs || !rhs)
      return false;
    auto lIdent = ast::getTypeIdentifier(lhs),
         rIdent = ast::getTypeIdentifier(rhs);
    if (rIdent == "_null" && !std::dynamic_pointer_cast<ast::BuiltinType>(lhs))
      return true;
    return lIdent == rIdent;
  }

  void checkVarDecl(const ast::VarDeclStmt &node) const {
    if (node.identifier->val == "this")
      throw CompileError(node.posBeg, node.posEnd);
    visit(node.type);
    if (!node.initExpr)
      return;
    visit(node.initExpr);
    if (!assignable(node.type, node.initExpr->type))
      throw IncompatibleTypes(node.posBeg, node.posEnd);
  }

  ast::ScopeID scopeResiding;
  ast::SymTbl &syms;
  bool inLoop;
  bool inFunc;
  std::shared_ptr<ast::Type> retType;

  std::shared_ptr<ast::BuiltinType> builtinInt, builtinBool, builtinString,
      builtinNull;
};

} // namespace mocker

namespace mocker {

SemanticChecker::SemanticChecker(std::shared_ptr<ast::ASTRoot> ast)
    : ast(std::move(ast)) {}

ast::SymTbl SemanticChecker::annotate() {
  ast::SymTbl syms;

  initSymTbl(syms);
  // User defined types, free functions and global variables
  collectSymbols(ast->decls, syms.global(), syms);

  // member functions and variables
  for (auto &decl : ast->decls) {
    if (auto classDecl = std::dynamic_pointer_cast<ast::ClassDecl>(decl)) {
      classDecl->scopeIntroduced = syms.createSubscope(syms.global());
      auto declThis = std::make_shared<ast::VarDecl>(
          mk_ast::pos(), mk_ast::pos(),
          std::make_shared<ast::VarDeclStmt>(
              mk_ast::pos(), mk_ast::pos(),
              std::make_shared<ast::UserDefinedType>(
                  mk_ast::pos(), mk_ast::pos(), classDecl->identifier),
              mk_ast::ident("this"), nullptr));
      syms.addSymbol(classDecl->scopeIntroduced, "this", declThis);
      collectSymbols(classDecl->members, classDecl->scopeIntroduced, syms);
      std::string ctorIdent = "_ctor_" + classDecl->identifier->val;
      if (!syms.lookUp(classDecl->scopeIntroduced, ctorIdent)) {
        syms.addSymbol(classDecl->scopeIntroduced, ctorIdent,
                       std::make_shared<ast::FuncDecl>(
                           mk_ast::pos(), mk_ast::pos(), nullptr,
                           mk_ast::ident(ctorIdent), mk_ast::emptyParams(),
                           mk_ast::body()));
      }
    }
  }

  ast->accept(Annotator(syms.global(), syms, false, false, nullptr));

  auto mainDef = std::dynamic_pointer_cast<ast::FuncDecl>(
      syms.lookUp(syms.global(), "main"));
  if (!mainDef || !mainDef->formalParameters.empty())
    throw InvalidMainFunction(mk_ast::pos(), mk_ast::pos());
  auto retTy = std::dynamic_pointer_cast<ast::BuiltinType>(mainDef->retType);
  if (!retTy || retTy->type != ast::BuiltinType::Int)
    throw InvalidMainFunction(mainDef->posBeg, mainDef->posEnd);

  return syms;
}

void SemanticChecker::collectSymbols(
    const std::vector<std::shared_ptr<ast::Declaration>> &decls,
    const ast::ScopeID &scope, ast::SymTbl &syms) {
  class Collect : public ast::Visitor {
  public:
    Collect(const ast::ScopeID &scope, ast::SymTbl &syms)
        : scope(scope), syms(syms) {}

    void operator()(ast::ClassDecl &decl) const override {
      auto res = syms.addSymbol(
          syms.global(), decl.identifier->val,
          std::static_pointer_cast<ast::ClassDecl>(decl.shared_from_this()));
      if (!res)
        throw DuplicatedSymbols(decl.posBeg, decl.posEnd, decl.identifier->val);
    }

    void operator()(ast::FuncDecl &decl) const override {
      std::string funcName = decl.identifier->val;
      auto res = syms.addSymbol(
          scope, funcName,
          std::static_pointer_cast<ast::FuncDecl>(decl.shared_from_this()));
      if (!res)
        throw DuplicatedSymbols(decl.posBeg, decl.posEnd, funcName);
    }

    void operator()(ast::VarDecl &decl) const override {
#ifdef DISABLE_FORWARD_REFERENCE_FOR_GLOBAL_VAR
      if (scope == syms.global())
        return;
#endif

      std::string varName = decl.decl->identifier->val;
      auto res = syms.addSymbol(
          scope, varName,
          std::static_pointer_cast<ast::VarDecl>(decl.shared_from_this()));
      if (!res)
        throw DuplicatedSymbols(decl.posBeg, decl.posEnd, varName);
    }

  private:
    const ast::ScopeID &scope;
    ast::SymTbl &syms;
  };

  for (const auto &decl : decls)
    decl->accept(Collect(scope, syms));
}

void SemanticChecker::initSymTbl(ast::SymTbl &syms) {
  using namespace mk_ast;
  // builtin functions
  // clang-format off
  syms.addSymbol(syms.global(), "print", std::make_shared<ast::FuncDecl>(
      pos(), pos(), nullptr, ident("print"),
      std::vector<FormalParam>({param(tyString())}), body()));
  syms.addSymbol(syms.global(), "println", std::make_shared<ast::FuncDecl>(
      pos(), pos(), nullptr, ident("println"),
      std::vector<FormalParam>({param(tyString())}), body()));
  syms.addSymbol(syms.global(), "getString", std::make_shared<ast::FuncDecl>(
      pos(), pos(), tyString(), ident("getString"), emptyParams(), body()));
  syms.addSymbol(syms.global(), "getInt", std::make_shared<ast::FuncDecl>(
      pos(), pos(), tyInt(), ident("getInt"), emptyParams(), body()));
  syms.addSymbol(syms.global(), "toString", std::make_shared<ast::FuncDecl>(
      pos(), pos(), tyString(), ident("toString"),
      std::vector<FormalParam>({param(tyInt())}), body()));

  // member functions of string
  syms.addSymbol(
      syms.global(), "_string_length", std::make_shared<ast::FuncDecl>(
          pos(), pos(), tyInt(), ident("length"),
          emptyParams(), body()));
  syms.addSymbol(
      syms.global(), "_string_substring", std::make_shared<ast::FuncDecl>(
          pos(), pos(), tyString(), ident("substring"),
          std::vector<FormalParem>({param(tyInt()), param(tyInt())}), body()));
  syms.addSymbol(
      syms.global(), "_string_parseInt", std::make_shared<ast::FuncDecl>(
          pos(), pos(), tyInt(), ident("parseInt"), emptyParams(), body()));
  syms.addSymbol(
      syms.global(), "_string_ord", std::make_shared<ast::FuncDecl>(
          pos(), pos(), tyInt(), ident("ord"),
          std::vector<FormalParem>({param(tyInt())}), body()));
  // member function of array
  syms.addSymbol(syms.global(), "_array_size", std::make_shared<ast::FuncDecl>(
      pos(), pos(), tyInt(), ident("size"), emptyParams(), body()));
  // clang-format on
}

} // namespace mocker
