#ifndef MOCKER_AST_VISITOR_H
#define MOCKER_AST_VISITOR_H

#include "fwd.h"

#include <cassert>
#include <type_traits>

namespace mocker {
namespace ast {

namespace detail {

template <bool AddConst, bool DefaultPass = true> class VisitorBase {
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
  virtual void operator()(disp<Identifier>) const { assert(DefaultPass); }

  virtual void operator()(disp<BuiltinType>) const { assert(DefaultPass); }
  virtual void operator()(disp<UserDefinedType>) const { assert(DefaultPass); }
  virtual void operator()(disp<ArrayType>) const { assert(DefaultPass); }

  virtual void operator()(disp<IntLitExpr>) const { assert(DefaultPass); }
  virtual void operator()(disp<BoolLitExpr>) const { assert(DefaultPass); }
  virtual void operator()(disp<StringLitExpr>) const { assert(DefaultPass); }
  virtual void operator()(disp<NullLitExpr>) const { assert(DefaultPass); }
  virtual void operator()(disp<IdentifierExpr>) const { assert(DefaultPass); }
  virtual void operator()(disp<UnaryExpr>) const { assert(DefaultPass); }
  virtual void operator()(disp<BinaryExpr>) const { assert(DefaultPass); }
  virtual void operator()(disp<FuncCallExpr>) const { assert(DefaultPass); }
  virtual void operator()(disp<NewExpr>) const { assert(DefaultPass); }

  virtual void operator()(disp<VarDeclStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<ExprStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<ReturnStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<ContinueStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<BreakStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<CompoundStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<IfStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<WhileStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<ForStmt>) const { assert(DefaultPass); }
  virtual void operator()(disp<EmptyStmt>) const { assert(DefaultPass); }

  virtual void operator()(disp<VarDecl>) const { assert(DefaultPass); }
  virtual void operator()(disp<FuncDecl>) const { assert(DefaultPass); }
  virtual void operator()(disp<ClassDecl>) const { assert(DefaultPass); }

  virtual void operator()(disp<ASTRoot>) const { assert(DefaultPass); }
};

} // namespace detail

class Visitor : public detail::VisitorBase<false> {};

class ConstVisitor : public detail::VisitorBase<true> {};

} // namespace ast
} // namespace mocker

#endif // MOCKER_AST_VISITOR_H
