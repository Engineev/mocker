#include "semantic_checker.h"

#include <utility>

#include "ast/ast_node.h"
#include "ast/helper.h"
#include "ast/visitor.h"
#include "error.h"
#include "semantic_context.h"
#include "sym_tbl.h"

// annotate
namespace mocker {

class Annotator : public ast::Visitor {
public:
  Annotator(ScopeID scopeResiding, SemanticContext &ctx,
            std::unordered_map<ast::NodeID, PosPair> &pos, bool inLoop,
            bool inFunc, std::shared_ptr<ast::Type> retType)
      : scopeResiding(std::move(scopeResiding)), ctx(ctx), pos(pos),
        inLoop(inLoop), inFunc(inFunc), retType(std::move(retType)) {
    builtinBool = mk_ast::tyBool();
    builtinInt = mk_ast::tyInt();
    builtinNull = mk_ast::tyNull();
    builtinString = mk_ast::tyString();
  }

  void operator()(ast::Identifier &node) const override {
    // This function is used to update ctx.scopeResiding only
    ctx.scopeResiding[node.getID()] = scopeResiding;
  }

  void operator()(ast::BuiltinType &node) const override { /* empty */
  }

  void operator()(ast::UserDefinedType &node) const override {
    if (!std::dynamic_pointer_cast<ast::ClassDecl>(
            ctx.syms.lookUp(scopeResiding, node.name->val)))
      throw UnresolvableSymbol(pos.at(node.getID()), node.name->val);
  }

  void operator()(ast::ArrayType &node) const override { visit(node.baseType); }

  void operator()(ast::IntLitExpr &node) const override {
    ctx.exprType[node.getID()] = builtinInt;
  }

  void operator()(ast::BoolLitExpr &node) const override {
    ctx.exprType[node.getID()] = builtinBool;
  }

  void operator()(ast::StringLitExpr &node) const override {
    ctx.exprType[node.getID()] = builtinString;
  }

  void operator()(ast::NullLitExpr &node) const override {
    ctx.exprType[node.getID()] = builtinNull;
  }

  void operator()(ast::IdentifierExpr &node) const override {
    visit(node.identifier);
    auto decl = std::dynamic_pointer_cast<ast::VarDecl>(
        ctx.syms.lookUp(scopeResiding, node.identifier->val));
    if (!decl)
      throw UnresolvableSymbol(pos.at(node.getID()), node.identifier->val);
    ctx.exprType[node.getID()] = decl->decl->type;
    if (node.identifier->val != "this")
      ctx.leftValue.insert(node.getID());
  }

  void operator()(ast::UnaryExpr &node) const override {
    visit(node.operand);
    if (ast::UnaryExpr::LogicalNot == node.op) {
      if (!assignable(builtinBool, ctx.exprType[node.operand->getID()]))
        throw IncompatibleTypes(pos.at(node.getID()));
    } else if (!assignable(builtinInt, ctx.exprType[node.operand->getID()])) {
      throw IncompatibleTypes(pos.at(node.getID()));
    }
    if (ast::UnaryExpr::PreDec == node.op ||
        ast::UnaryExpr::PreInc == node.op) {
      ctx.leftValue.insert(node.getID());
      if (!ctx.isLeftValue(node.operand->getID()))
        throw InvalidRightValueOperation(pos.at(node.getID()));
    }
    ctx.exprType[node.getID()] = ctx.exprType[node.operand->getID()];
  }

  void operator()(ast::BinaryExpr &node) const override {
    auto ce = [this, &node] { throw IncompatibleTypes(pos.at(node.getID())); };
    auto ceRv = [this, &node] {
      throw InvalidRightValueOperation(pos.at(node.getID()));
    };

    visit(node.lhs);
    if (node.op != ast::BinaryExpr::Member)
      visit(node.rhs);

    auto lhsT = ctx.exprType[node.lhs->getID()],
         rhsT = ctx.exprType[node.rhs->getID()];

    switch (node.op) {
    case ast::BinaryExpr::Assign:
      if (!assignable(lhsT, rhsT))
        ce();
      if (!ctx.isLeftValue(node.lhs->getID()))
        ceRv();
      ctx.exprType[node.getID()] = lhsT;
      return;
    case ast::BinaryExpr::LogicalOr:
    case ast::BinaryExpr::LogicalAnd:
      if (!assignable(builtinBool, lhsT) || !assignable(builtinBool, rhsT))
        ce();
      ctx.exprType[node.getID()] = builtinBool;
      return;
    case ast::BinaryExpr::Add:
      if (!assignable(lhsT, rhsT))
        ce();
      if (assignable(builtinInt, lhsT)) {
        ctx.exprType[node.getID()] = builtinInt;
        return;
      } else if (assignable(builtinString, lhsT)) {
        ctx.exprType[node.getID()] = builtinString;
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
      if (!assignable(builtinInt, lhsT) || !assignable(builtinInt, rhsT))
        ce();
      ctx.exprType[node.getID()] = builtinInt;
      return;
    case ast::BinaryExpr::Eq:
    case ast::BinaryExpr::Ne:
      ctx.exprType[node.getID()] = builtinBool;
      if (!assignable(lhsT, rhsT))
        ce();
      if (std::dynamic_pointer_cast<ast::NullLitExpr>(node.lhs) &&
          std::dynamic_pointer_cast<ast::NullLitExpr>(node.rhs))
        return;
      if (std::dynamic_pointer_cast<ast::NullLitExpr>(node.lhs) ||
          std::dynamic_pointer_cast<ast::NullLitExpr>(node.rhs)) {
        auto ty = (bool)std::dynamic_pointer_cast<ast::NullLitExpr>(node.lhs)
                      ? rhsT
                      : lhsT;
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
      if (!assignable(lhsT, rhsT))
        ce();
      if (!assignable(builtinInt, lhsT) && !assignable(builtinString, lhsT) &&
          !assignable(builtinBool, lhsT))
        ce();
      ctx.exprType[node.getID()] = builtinBool;
      return;
    case ast::BinaryExpr::Subscript: {
      auto arrayTy = std::dynamic_pointer_cast<ast::ArrayType>(lhsT);
      if (!arrayTy)
        throw CompileError(pos.at(node.lhs->getID()));
      if (!assignable(builtinInt, rhsT))
        ce();
      ctx.exprType[node.getID()] = arrayTy->baseType;
      ctx.leftValue.insert(node.getID());
      return;
    }
    case ast::BinaryExpr::Member:
      if (auto ptr = std::dynamic_pointer_cast<ast::UserDefinedType>(lhsT)) {
        auto decl = std::dynamic_pointer_cast<ast::ClassDecl>(
            ctx.syms.lookUp(scopeResiding, ptr->name->val));
        auto identExpr =
            std::dynamic_pointer_cast<ast::IdentifierExpr>(node.rhs);
        if (!identExpr)
          ce();
        auto memberVarDecl = std::dynamic_pointer_cast<ast::VarDecl>(
            ctx.syms.lookUp(ctx.scopeIntroduced[decl->getID()],
                            identExpr->identifier->val));
        if (!memberVarDecl)
          ce();
        ctx.exprType[node.getID()] = memberVarDecl->decl->type;
      } else
        ce();
      ctx.leftValue.insert(node.getID());
      return;
    }
  }

  void operator()(ast::FuncCallExpr &node) const override {
    visit(node.identifier);
    std::shared_ptr<ast::FuncDecl> funcDecl;
    bool isCtor = false;
    if (node.instance) { // is member functional call
      visit(node.instance);
      if (assignable(builtinString, ctx.exprType[node.instance->getID()])) {
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(ctx.syms.lookUp(
            ctx.syms.global(), "_string_" + node.identifier->val));
        assert(funcDecl);
      } else if (std::dynamic_pointer_cast<ast::ArrayType>(
                     ctx.exprType[node.instance->getID()])) {
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(ctx.syms.lookUp(
            ctx.syms.global(), "_array_" + node.identifier->val));
        assert(funcDecl);
      } else {
        auto instType = std::dynamic_pointer_cast<ast::UserDefinedType>(
            ctx.exprType[node.instance->getID()]);
        assert(instType);
        auto classDecl =
            std::dynamic_pointer_cast<ast::ClassDecl>(ctx.syms.lookUp(
                ctx.syms.global(), ast::getTypeIdentifier(instType)));
        assert(classDecl);
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(ctx.syms.lookUp(
            ctx.scopeIntroduced[classDecl->getID()], node.identifier->val));
        ctx.scopeResiding[node.identifier->getID()] =
            ctx.scopeIntroduced[classDecl->getID()];
      }
    } else { // is not member function call
      auto decl = ctx.syms.lookUp(scopeResiding, node.identifier->val);
      if (auto ptr =
              std::dynamic_pointer_cast<ast::ClassDecl>(decl)) { // is ctor
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(
            ctx.syms.lookUp(ctx.scopeIntroduced[ptr->getID()], "_ctor_"));
        isCtor = true;
        auto tmp = std::make_shared<ast::UserDefinedType>(ptr->identifier);
        pos[tmp->getID()] = pos[ptr->getID()];
        ctx.exprType[node.getID()] = tmp;
      } else { // is not ctor
        funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(decl);
      }
    }
    if (!funcDecl)
      throw UnresolvableSymbol(pos.at(node.identifier->getID()),
                               node.identifier->val);

    if (node.args.size() != funcDecl->formalParameters.size())
      throw InvalidFunctionCall(pos.at(node.getID()));
    for (std::size_t i = 0; i < node.args.size(); ++i) {
      visit(node.args[i]);
      if (!assignable(funcDecl->formalParameters[i]->type,
                      ctx.exprType[node.args[i]->getID()]))
        throw InvalidFunctionCall(pos.at(node.getID()));
    }
    if (!isCtor)
      ctx.exprType[node.getID()] = funcDecl->retType;
  }

  void operator()(ast::NewExpr &node) const override {
    ctx.exprType[node.getID()] = node.type;
    for (auto &expr : node.providedDims) {
      visit(expr);
      if (!assignable(builtinInt, ctx.exprType[expr->getID()]))
        throw IncompatibleTypes(pos.at(expr->getID()));
    }
  }

  void operator()(ast::VarDeclStmt &node) const override {
    visit(node.identifier);

    checkVarDecl(node);
    auto decl = std::make_shared<ast::VarDecl>(
        std::static_pointer_cast<ast::VarDeclStmt>(node.shared_from_this()));
    pos[decl->getID()] = pos[node.getID()];
    bool addRes = ctx.syms.addSymbol(scopeResiding, node.identifier->val, decl);

    if (!addRes)
      throw DuplicatedSymbols(pos[node.getID()], node.identifier->val);
  }

  void operator()(ast::ExprStmt &node) const override { visit(node.expr); }

  void operator()(ast::ReturnStmt &node) const override {
    if (!inFunc)
      throw ReturnOutOfAFunction(pos.at(node.getID()));
    if (!node.expr) {
      if (!retType)
        return;
      throw IncompatibleTypes(pos.at(node.getID()));
    }

    visit(node.expr);
    if (!assignable(retType, ctx.exprType[node.expr->getID()]))
      throw IncompatibleTypes(pos.at(node.expr->getID()));
  }

  void operator()(ast::ContinueStmt &node) const override {
    if (!inLoop)
      throw ContinueOutOfALoop(pos.at(node.getID()));
  }

  void operator()(ast::BreakStmt &node) const override {
    if (!inLoop)
      throw BreakOutOfALoop(pos.at(node.getID()));
  }

  void operator()(ast::CompoundStmt &node) const override {
    auto scopeIntroduced = ctx.syms.createSubscope(scopeResiding);
    for (auto &stmt : node.stmts)
      visit(stmt, scopeIntroduced);
  }

  void operator()(ast::IfStmt &node) const override {
    visit(node.condition);
    if (!assignable(builtinBool, ctx.exprType[node.condition->getID()]))
      throw IncompatibleTypes(pos.at(node.condition->getID()));
    auto thenScope = ctx.syms.createSubscope(scopeResiding);
    visit(node.then, thenScope);
    if (node.else_) {
      auto elseScope = ctx.syms.createSubscope(scopeResiding);
      visit(node.else_, elseScope);
    }
  }

  void operator()(ast::WhileStmt &node) const override {
    visit(node.condition, scopeResiding);
    if (!assignable(builtinBool, ctx.exprType[node.condition->getID()]))
      throw IncompatibleTypes(pos.at(node.condition->getID()));

    auto scopeIntroduced = ctx.syms.createSubscope(scopeResiding);
    visit(node.body, scopeIntroduced, true, inFunc, retType);
  }

  void operator()(ast::ForStmt &node) const override {
    if (node.init)
      visit(node.init, scopeResiding);
    if (node.condition) {
      visit(node.condition, scopeResiding);
      if (!assignable(builtinBool, ctx.exprType[node.condition->getID()]))
        throw IncompatibleTypes(pos.at(node.condition->getID()));
    }
    if (node.update)
      visit(node.update, scopeResiding);

    auto scopeIntroduced = ctx.syms.createSubscope(scopeResiding);
    visit(node.body, scopeIntroduced, true, inFunc, retType);
  }

  void operator()(ast::EmptyStmt &node) const override { /* pass */
  }

  void operator()(ast::VarDecl &node) const override { visit(node.decl); }

  void operator()(ast::FuncDecl &node) const override {
    if (node.retType)
      visit(node.retType);

    auto scopeIntroduced = ctx.syms.createSubscope(scopeResiding);

    for (const auto &param : node.formalParameters) {
      visit(param, scopeIntroduced);
    }

    for (auto &stmt : node.body->stmts) {
      visit(stmt, scopeIntroduced, false, true, node.retType);
    }
  }

  void operator()(ast::ClassDecl &node) const override {
    auto scopeIntroduced = ctx.scopeIntroduced[node.getID()];
    for (auto &member : node.members) {
      if (auto ptr = std::dynamic_pointer_cast<ast::VarDecl>(member)) {
        visit(ptr->decl->identifier);
        checkVarDecl(*ptr->decl);
        continue;
      }
      visit(member, scopeIntroduced);
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
      visit(decl, ctx.syms.global());
    }
  }

private:
  void visit(const std::shared_ptr<ast::ASTNode> &node) const {
    visit(node, scopeResiding);
  }

  void visit(const std::shared_ptr<ast::ASTNode> &node,
             ScopeID residing) const {
    visit(node, std::move(residing), inLoop, inFunc, retType);
  }

  void visit(const std::shared_ptr<ast::ASTNode> &node, ScopeID residing,
             bool inLoop, bool inFunc,
             std::shared_ptr<ast::Type> retType) const {
    node->accept(Annotator(std::move(residing), ctx, pos, inLoop, inFunc,
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
      throw CompileError(pos.at(node.getID()));
    visit(node.type);
    if (!node.initExpr)
      return;
    visit(node.initExpr);
    if (!assignable(node.type, ctx.exprType[node.initExpr->getID()]))
      throw IncompatibleTypes(pos.at(node.getID()));
  }

  ScopeID scopeResiding;
  SemanticContext &ctx;
  std::unordered_map<ast::NodeID, PosPair> &pos;
  bool inLoop;
  bool inFunc;
  std::shared_ptr<ast::Type> retType;

  std::shared_ptr<ast::BuiltinType> builtinInt, builtinBool, builtinString,
      builtinNull;
};

} // namespace mocker

namespace mocker {

SemanticChecker::SemanticChecker(std::shared_ptr<ast::ASTRoot> ast,
                                 std::unordered_map<ast::NodeID, PosPair> &pos)
    : ast(std::move(ast)), pos(pos) {}

void SemanticChecker::check() {
  if (ctx.state != SemanticContext::State::Unused)
    std::terminate();

  ctx.state = SemanticContext::State::Dirty;

  initSymTbl(ctx.syms);
  // User defined types, free functions and global variables
  collectSymbols(ast->decls, ctx.syms.global(), ctx.syms);

  // member functions and variables
  for (auto &decl : ast->decls) {
    if (auto classDecl = std::dynamic_pointer_cast<ast::ClassDecl>(decl)) {
      const auto ClassScope = ctx.syms.createSubscope(ctx.syms.global());
      ctx.scopeIntroduced[classDecl->getID()] = ClassScope;
      auto declThis =
          std::make_shared<ast::VarDecl>(std::make_shared<ast::VarDeclStmt>(
              std::make_shared<ast::UserDefinedType>(classDecl->identifier),
              mk_ast::ident("this"), nullptr));
      ctx.syms.addSymbol(ClassScope, "this", declThis);
      collectSymbols(classDecl->members, ClassScope, ctx.syms);

      auto p = ctx.syms.lookUp(ClassScope, classDecl->identifier->val);
      if (p && std::dynamic_pointer_cast<ast::FuncDecl>(p))
        throw CompileError(pos.at(classDecl->getID()));

      std::string ctorIdent = "_ctor_";
      if (!ctx.syms.lookUp(ClassScope, ctorIdent)) {
        auto ctor = std::make_shared<ast::FuncDecl>(
            nullptr, mk_ast::ident(ctorIdent), mk_ast::emptyParams(),
            std::make_shared<ast::CompoundStmt>(
                std::vector<std::shared_ptr<ast::Statement>>()));
        ctx.syms.addSymbol(ClassScope, ctorIdent, ctor);
        classDecl->members.emplace_back(ctor);
      }
    }
  }

  ast->accept(Annotator(ctx.syms.global(), ctx, pos, false, false, nullptr));

  auto mainDef = std::dynamic_pointer_cast<ast::FuncDecl>(
      ctx.syms.lookUp(ctx.syms.global(), "main"));
  if (!mainDef || !mainDef->formalParameters.empty())
    throw InvalidMainFunction(Position(), Position());
  auto retTy = std::dynamic_pointer_cast<ast::BuiltinType>(mainDef->retType);
  if (!retTy || retTy->type != ast::BuiltinType::Int)
    throw InvalidMainFunction(pos.at(mainDef->getID()));

  ctx.state = SemanticContext::State::Completed;
}

void SemanticChecker::collectSymbols(
    const std::vector<std::shared_ptr<ast::Declaration>> &decls,
    const ScopeID &scope, SymTbl &syms) {
  class Collect : public ast::Visitor {
  public:
    Collect(const ScopeID &scope, SymTbl &syms,
            const std::unordered_map<ast::NodeID, PosPair> &pos)
        : scope(scope), syms(syms), pos(pos) {}

    void operator()(ast::ClassDecl &decl) const override {
      auto res = syms.addSymbol(
          syms.global(), decl.identifier->val,
          std::static_pointer_cast<ast::ClassDecl>(decl.shared_from_this()));
      if (!res)
        throw DuplicatedSymbols(pos.at(decl.getID()), decl.identifier->val);
    }

    void operator()(ast::FuncDecl &decl) const override {
      std::string funcName = decl.identifier->val;
      auto res = syms.addSymbol(
          scope, funcName,
          std::static_pointer_cast<ast::FuncDecl>(decl.shared_from_this()));
      if (!res)
        throw DuplicatedSymbols(pos.at(decl.getID()), funcName);
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
        throw DuplicatedSymbols(pos.at(decl.getID()), varName);
    }

  private:
    const ScopeID &scope;
    SymTbl &syms;
    const std::unordered_map<ast::NodeID, PosPair> &pos;
  };

  for (const auto &decl : decls)
    decl->accept(Collect(scope, syms, pos));
}

void SemanticChecker::initSymTbl(SymTbl &syms) {
  using namespace mk_ast;
  using FormalParam = std::shared_ptr<ast::VarDeclStmt>;
  // builtin functions
  // clang-format off
  syms.addSymbol(syms.global(), "print", std::make_shared<ast::FuncDecl>(nullptr, ident("print"),
      std::vector<FormalParam>({param(tyString())}), body()));
  syms.addSymbol(syms.global(), "println", std::make_shared<ast::FuncDecl>(
      nullptr, ident("println"),
      std::vector<FormalParam>({param(tyString())}), body()));
  syms.addSymbol(syms.global(), "getString", std::make_shared<ast::FuncDecl>(
       tyString(), ident("getString"), emptyParams(), body()));
  syms.addSymbol(syms.global(), "getInt", std::make_shared<ast::FuncDecl>(
       tyInt(), ident("getInt"), emptyParams(), body()));
  syms.addSymbol(syms.global(), "toString", std::make_shared<ast::FuncDecl>(
       tyString(), ident("toString"),
      std::vector<FormalParam>({param(tyInt())}), body()));

  // member functions of string
  syms.addSymbol(
      syms.global(), "_string_length", std::make_shared<ast::FuncDecl>(
           tyInt(), ident("length"),
          emptyParams(), body()));
  syms.addSymbol(
      syms.global(), "_string_substring", std::make_shared<ast::FuncDecl>(
           tyString(), ident("substring"),
          std::vector<FormalParam>({param(tyInt()), param(tyInt())}), body()));
  syms.addSymbol(
      syms.global(), "_string_parseInt", std::make_shared<ast::FuncDecl>(
           tyInt(), ident("parseInt"), emptyParams(), body()));
  syms.addSymbol(
      syms.global(), "_string_ord", std::make_shared<ast::FuncDecl>(
           tyInt(), ident("ord"),
          std::vector<FormalParam>({param(tyInt())}), body()));
  // member function of array
  syms.addSymbol(syms.global(), "_array_size", std::make_shared<ast::FuncDecl>(
       tyInt(), ident("size"), emptyParams(), body()));
  // clang-format on
}

void SemanticChecker::renameIdentifiers() {
  if (ctx.state != SemanticContext::State::Completed)
    std::terminate();

  class Rename : public ast::Visitor {
  public:
    explicit Rename(const SemanticContext &ctx) : ctx(ctx) {}

    void operator()(ast::Identifier &node) const override { assert(false); }
    void operator()(ast::IdentifierExpr &node) const override {
      auto decl = std::dynamic_pointer_cast<ast::VarDecl>(
          ctx.syms.lookUp(ctx.scopeResiding.at(node.identifier->getID()),
                          node.identifier->val));
      node.identifier->val = decl->decl->identifier->val;
    }
    void operator()(ast::UnaryExpr &node) const override {
      visit(node.operand);
    }
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
              ctx.exprType.at(node.instance->getID()))) {
        assert(ident == "size");
        ident = "#_array_#size";
        return;
      }
      if (auto p = std::dynamic_pointer_cast<ast::BuiltinType>(
              ctx.exprType.at(node.instance->getID()))) {
        assert(p->type == ast::BuiltinType::String);
        ident = "#string#" + ident;
        return;
      }

      auto funcDecl = std::dynamic_pointer_cast<ast::FuncDecl>(
          ctx.syms.lookUp(ctx.scopeResiding.at(node.identifier->getID()),
                          node.identifier->val));
      assert(funcDecl);
      ident = funcDecl->identifier->val;
    }
    void operator()(ast::NewExpr &node) const override {
      for (auto &exp : node.providedDims)
        visit(exp);
    }
    void operator()(ast::VarDeclStmt &node) const override {
      renameLocalVar(node);
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
          memberVars.emplace_back(std::move(p));

      for (auto &var : memberVars)
        renameMemberVar(*var, node.identifier->val);
      for (auto &var : memberVars)
        if (var->decl->initExpr)
          visit(var->decl->initExpr);

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
          globalVars.emplace_back(std::move(p));

      for (auto &var : globalVars)
        renameGlobalVar(*var);
      for (auto &var : globalVars)
        if (var->decl->initExpr)
          visit(var->decl->initExpr);

      for (auto &decl : node.decls) {
        if (std::dynamic_pointer_cast<ast::VarDecl>(decl))
          continue;
        visit(decl);
      }
    }

  private:
    void visit(const std::shared_ptr<ast::ASTNode> &p) const {
      p->accept(*this);
    }

    void renameGlobalVar(ast::VarDecl &node) const {
      auto oldIdent = node.decl->identifier->val;
      node.decl->identifier->val = "@" + oldIdent;
    }

    void renameMemberVar(ast::VarDecl &node,
                         const std::string &className) const {
      auto oldIdent = node.decl->identifier->val;
      node.decl->identifier->val = "#" + className + "#" + oldIdent;
    }

    void renameLocalVar(ast::VarDeclStmt &node) const {
      auto scopeInfo = ctx.scopeResiding.at(node.identifier->getID()).fmt();
      assert(scopeInfo.find('_') == std::string::npos);
      node.identifier->val = scopeInfo + '_' + node.identifier->val;
    }

    void renameMemberFunc(ast::FuncDecl &node,
                          const std::string &className) const {
      auto oldIdent = node.identifier->val;
      node.identifier->val = "#" + className + "#" + oldIdent;
    }

    const SemanticContext &ctx;
  };

  ast->accept(Rename(ctx));
}

} // namespace mocker
