#ifndef MOCKER_BUILDER_H
#define MOCKER_BUILDER_H

#include <cassert>
#include <queue>
#include <utility>

#include "ast/ast_node.h"
#include "ast/visitor.h"
#include "builder_context.h"

namespace mocker {
namespace ir {

class Builder : virtual public ast::ConstVisitor {
public:
  explicit Builder(BuilderContext &ctx) : ctx(ctx) {}

  void operator()(const ast::Identifier &node) const override { assert(false); }
  void operator()(const ast::BuiltinType &node) const override {
    assert(false);
  }
  void operator()(const ast::UserDefinedType &node) const override {
    assert(false);
  }
  void operator()(const ast::ArrayType &node) const override { assert(false); }

  void operator()(const ast::IntLitExpr &node) const override;

  void operator()(const ast::BoolLitExpr &node) const override;

  void operator()(const ast::StringLitExpr &node) const override;

  void operator()(const ast::NullLitExpr &node) const override;

  void operator()(const ast::IdentifierExpr &node) const override;

  void operator()(const ast::UnaryExpr &node) const override;

  void operator()(const ast::BinaryExpr &node) const override;

  void operator()(const ast::FuncCallExpr &node) const override;

  void operator()(const ast::NewExpr &node) const override;

  void operator()(const ast::VarDeclStmt &node) const override;

  void operator()(const ast::ExprStmt &node) const override;

  void operator()(const ast::ReturnStmt &node) const override;

  void operator()(const ast::ContinueStmt &node) const override;

  void operator()(const ast::BreakStmt &node) const override;

  void operator()(const ast::CompoundStmt &node) const override;

  void operator()(const ast::IfStmt &node) const override;

  void operator()(const ast::WhileStmt &node) const override;

  void operator()(const ast::ForStmt &node) const override;

  void operator()(const ast::EmptyStmt &node) const override {}

  void operator()(const ast::VarDecl &node) const override { assert(false); }

  void operator()(const ast::FuncDecl &node) const override;

  void operator()(const ast::ClassDecl &node) const override;

  void operator()(const ast::ASTRoot &node) const override;

private:
  void addClassLayout(const ast::ClassDecl &node) const;

  std::shared_ptr<Label> getBBLabel(BasicBlockList::const_iterator bb) const;

  template <class Node> void visit(const Node &node) const {
    node.accept(*this);
  }

  bool isIntTy(const std::shared_ptr<ast::Type> &ty) const {
    auto type = std::dynamic_pointer_cast<ast::BuiltinType>(ty);
    return (bool)type && type->type == ast::BuiltinType::Int;
  }
  bool isBoolTy(const std::shared_ptr<ast::Type> &ty) const {
    auto type = std::dynamic_pointer_cast<ast::BuiltinType>(ty);
    return (bool)type && type->type == ast::BuiltinType::Bool;
  }
  bool isNullTy(const std::shared_ptr<ast::Type> &ty) const {
    auto type = std::dynamic_pointer_cast<ast::BuiltinType>(ty);
    return (bool)type && type->type == ast::BuiltinType::Null;
  }
  bool isStrTy(const std::shared_ptr<ast::Type> &ty) const {
    auto type = std::dynamic_pointer_cast<ast::BuiltinType>(ty);
    return (bool)type && type->type == ast::BuiltinType::String;
  }

  // The behavior is undefined if [exp] is not an left value.
  // Return a register containing the address of the expression. If the
  // expression is of type bool or int, then it is just the address of the
  // value. For other cases, it is the address of the pointer pointing to the
  // actual instance.
  std::shared_ptr<Addr>
  getElementPtr(const std::shared_ptr<ast::ASTNode> &exp) const;

  std::pair<std::string, std::string>
  splitMemberVarIdent(const std::string &ident) const;

  void
  translateLoop(const std::shared_ptr<ast::Expression> &condition,
                const std::shared_ptr<ast::Statement> &body,
                const std::shared_ptr<ast::Expression> &update = nullptr) const;

  std::shared_ptr<Addr> /* contains the address of the array instance */
  translateNewArray(const std::shared_ptr<ast::Type> &rawCurType,
                    std::queue<std::shared_ptr<Addr>> &sizeProvided) const;

  void addBuiltinAndExternal() const;

  void addGlobalVariable(const std::shared_ptr<ast::VarDecl> &decl) const;

private:
  std::shared_ptr<Reg> getMemberElementPtr(const std::shared_ptr<Addr> &base,
                                           const std::string &className,
                                           const std::string &varName) const;

  std::shared_ptr<Reg> makeReg(std::string identifier) const;

  std::shared_ptr<Addr> /* contains the pointer to the actual instance */
  makeNewNonarray(const std::shared_ptr<ast::Type> &type) const;

  std::shared_ptr<Addr> makeILit(Integer val) const;

  std::string getTypeIdentifier(const std::shared_ptr<ast::Type> &type) const;

  std::size_t getTypeSize(const std::shared_ptr<ast::Type> &type) const;

private:
  BuilderContext &ctx;
};

} // namespace ir
} // namespace mocker

#endif // MOCKER_BUILDER_H
