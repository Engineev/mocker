#ifndef MOCKER_AST_VISITOR_H
#define MOCKER_AST_VISITOR_H

#include "fwd.h"

#include <type_traits>

namespace mocker {
namespace ast {

namespace detail {

template <bool AddConst> class VisitorBase {
private:
  template <class T, bool> struct Dispatch;

  template <class T> struct Dispatch<T, true> {
    using type = typename std::add_const<T>::type;
  };

  template <class T> struct Dispatch<T, false> {
    using type = typename std::remove_const<T>::type;
  };

  template <class T> using disp = typename Dispatch<T, AddConst>::type &;

public:
  virtual void operator()(disp<BuiltinType>) const = 0;
  virtual void operator()(disp<UserDefinedType>) const = 0;
  virtual void operator()(disp<ArrayType>) const = 0;

  virtual void operator()(disp<IntLitExpr>) const = 0;
  virtual void operator()(disp<BoolLitExpr>) const = 0;
  virtual void operator()(disp<StringLitExpr>) const = 0;
  virtual void operator()(disp<NullLitExpr>) const = 0;
  virtual void operator()(disp<IdentifierExpr>) const = 0;
  virtual void operator()(disp<UnaryExpr>) const = 0;
  virtual void operator()(disp<BinaryExpr>) const = 0;
  virtual void operator()(disp<FuncCallExpr>) const = 0;
  virtual void operator()(disp<NewExpr>) const = 0;

  virtual void operator()(disp<VarDeclStmt>) const = 0;
  virtual void operator()(disp<ExprStmt>) const = 0;
  virtual void operator()(disp<ReturnStmt>) const = 0;
  virtual void operator()(disp<ContinueStmt>) const = 0;
  virtual void operator()(disp<BreakStmt>) const = 0;
  virtual void operator()(disp<CompoundStmt>) const = 0;
  virtual void operator()(disp<IfStmt>) const = 0;
  virtual void operator()(disp<WhileStmt>) const = 0;
  virtual void operator()(disp<ForStmt>) const = 0;
  virtual void operator()(disp<EmptyStmt>) const = 0;

  virtual void operator()(disp<VarDecl>) const = 0;
  virtual void operator()(disp<FuncDecl>) const = 0;
  virtual void operator()(disp<ClassDecl>) const = 0;

  virtual void operator()(disp<ASTRoot>) const = 0;
};

} // namespace detail

class Visitor : public detail::VisitorBase<false> {};

class ConstVisitor : public detail::VisitorBase<true> {};

} // namespace ast
} // namespace mocker

#endif // MOCKER_AST_VISITOR_H
