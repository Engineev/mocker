#include <utility>

#ifndef MOCKER_AST_H
#define MOCKER_AST_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/position.h"
#include "fwd.h"
#include "scope_id.h"
#include "visitor.h"

#define MOCKER_PURE_ACCEPT                                                     \
  void accept(const Visitor &vis) override = 0;                                \
  void accept(const ConstVisitor &vis) const override = 0;

#define MOCKER_ACCEPT                                                          \
  void accept(const Visitor &vis) override { vis(*this); }                     \
  void accept(const ConstVisitor &vis) const override { vis(*this); }

namespace mocker {
namespace ast {

struct ASTNode : public std::enable_shared_from_this<ASTNode> {
  ASTNode() = default;
  ASTNode(const Position &posBeg, const Position &posEnd)
      : posBeg(posBeg), posEnd(posEnd) {}

  virtual ~ASTNode() = default;

  virtual void accept(const Visitor &vis) = 0;
  virtual void accept(const ConstVisitor &vis) const = 0;

  Position posBeg, posEnd;
};

/*- misc ---------------------------------------------------------------------*/

struct Identifier : ASTNode {
  Identifier(const Position &posBeg, const Position &posEnd, std::string val)
      : ASTNode(posBeg, posEnd), val(std::move(val)) {}

  MOCKER_ACCEPT
  std::string val;
};

struct Type : ASTNode {
  Type() = default;
  Type(const Position &posBeg, const Position &posEnd)
      : ASTNode(posBeg, posEnd) {}

  MOCKER_PURE_ACCEPT
};

struct NonarrayType : Type {
  NonarrayType() = default;
  NonarrayType(const Position &posBeg, const Position &posEnd)
      : Type(posBeg, posEnd) {}

  MOCKER_PURE_ACCEPT
};

struct BuiltinType : NonarrayType {
  enum Type { Null, Int, String, Bool };

  BuiltinType() = default;
  BuiltinType(const Position &posBeg, const Position &posEnd, Type type)
      : NonarrayType(posBeg, posEnd), type(type) {}

  MOCKER_ACCEPT

  Type type = Null;
};

struct UserDefinedType : NonarrayType {
  UserDefinedType() = default;
  UserDefinedType(const Position &posBeg, const Position &posEnd,
                  std::shared_ptr<Identifier> name)
      : NonarrayType(posBeg, posEnd), name(std::move(name)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Identifier> name;
};

struct ArrayType : Type {
  ArrayType() = default;
  ArrayType(const Position &posBeg, const Position &posEnd,
            std::shared_ptr<Type> baseType)
      : Type(posBeg, posEnd), baseType(std::move(baseType)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Type> baseType;
};

/*- expression ---------------------------------------------------------------*/

struct Expression : ASTNode {
  Expression() = default;
  Expression(const Position &posBeg, const Position &posEnd)
      : ASTNode(posBeg, posEnd) {}

  MOCKER_PURE_ACCEPT

  std::shared_ptr<Type> type;
};

struct LiteralExpr : Expression {
  LiteralExpr() = default;
  LiteralExpr(const Position &posBeg, const Position &posEnd)
      : Expression(posBeg, posEnd) {}

  MOCKER_PURE_ACCEPT
};

struct IntLitExpr : LiteralExpr {
  IntLitExpr() = default;
  IntLitExpr(const Position &posBeg, const Position &posEnd, Integer val)
      : LiteralExpr(posBeg, posEnd), val(val) {}

  MOCKER_ACCEPT

  Integer val = 0;
};

struct StringLitExpr : LiteralExpr {
  StringLitExpr() = default;
  StringLitExpr(const Position &posBeg, const Position &posEnd, std::string val)
      : LiteralExpr(posBeg, posEnd), val(std::move(val)) {}

  MOCKER_ACCEPT

  std::string val;
};

struct NullLitExpr : LiteralExpr {
  NullLitExpr() = default;
  NullLitExpr(const Position &posBeg, const Position &posEnd)
      : LiteralExpr(posBeg, posEnd) {}

  MOCKER_ACCEPT
};

struct BoolLitExpr : LiteralExpr {
  BoolLitExpr() = default;
  BoolLitExpr(const Position &posBeg, const Position &posEnd, bool val)
      : LiteralExpr(posBeg, posEnd), val(val) {}

  MOCKER_ACCEPT

  bool val = false;
};

struct IdentifierExpr : Expression {
  IdentifierExpr(const Position &posBeg, const Position &posEnd,
                 std::shared_ptr<Identifier> identifier)
      : Expression(posBeg, posEnd), identifier(std::move(identifier)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Identifier> identifier;
};

struct UnaryExpr : Expression {
  enum OpType { Inc, Dec, Neg, LogicalNot, BitNot };

  UnaryExpr(const Position &posBeg, const Position &posEnd, OpType op,
            std::shared_ptr<Expression> operand)
      : Expression(posBeg, posEnd), op(op), operand(std::move(operand)) {}

  MOCKER_ACCEPT

  OpType op = Inc;
  std::shared_ptr<Expression> operand;
};

struct BinaryExpr : Expression {
  // clang-format off
  enum OpType {
    Assign = 0,
    LogicalOr,
    LogicalAnd,
    BitOr,
    BitAnd,
    Xor,
    Eq, Ne, Lt, Gt, Le, Ge,
    Shl, Shr,
    Add, Sub,
    Mul, Div, Mod,
    Subscript, Member
  };
  // clang-format on

  BinaryExpr(const Position &posBeg, const Position &posEnd, OpType op,
             std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
      : Expression(posBeg, posEnd), op(op), lhs(std::move(lhs)),
        rhs(std::move(rhs)) {}

  MOCKER_ACCEPT

  OpType op = Assign;
  std::shared_ptr<Expression> lhs, rhs;
};

struct FuncCallExpr : Expression {
  FuncCallExpr(const Position &posBeg, const Position &posEnd,
               std::shared_ptr<Expression> instance,
               std::shared_ptr<Identifier> identifier,
               std::vector<std::shared_ptr<Expression>> args)
      : Expression(posBeg, posEnd), instance(std::move(instance)),
        identifier(std::move(identifier)), args(std::move(args)) {}

  MOCKER_ACCEPT

  // e.g. In a[0].foo(1, 2), [instance] is a[0], [identifier] is foo and [args]
  // is 1, 2. If it is a free function call, then [instance] is empty.
  std::shared_ptr<Expression> instance;
  std::shared_ptr<Identifier> identifier;
  std::vector<std::shared_ptr<Expression>> args;
};

struct NewExpr : Expression {
  NewExpr(const Position &posBeg, const Position &posEnd,
          std::shared_ptr<Type> type_,
          std::vector<std::shared_ptr<Expression>> providedDims)
      : Expression(posBeg, posEnd), providedDims(std::move(providedDims)) {
    type = std::move(type_);
  }

  MOCKER_ACCEPT

  // std::shared_ptr<Type> type; inherit from Expression
  std::vector<std::shared_ptr<Expression>> providedDims;
};

/*- statement ----------------------------------------------------------------*/

struct Statement : ASTNode {
  Statement(const Position &posBeg, const Position &posEnd)
      : ASTNode(posBeg, posEnd) {}

  MOCKER_PURE_ACCEPT
};

struct VarDeclStmt : Statement {
  VarDeclStmt(const Position &posBeg, const Position &posEnd,
              std::shared_ptr<Type> type,
              std::shared_ptr<ast::Identifier> identifier,
              std::shared_ptr<Expression> initExpr)
      : Statement(posBeg, posEnd), type(std::move(type)),
        identifier(std::move(identifier)), initExpr(std::move(initExpr)) {}
  MOCKER_ACCEPT

  std::shared_ptr<Type> type;
  std::shared_ptr<Identifier> identifier;
  std::shared_ptr<Expression> initExpr;
};

struct ExprStmt : Statement {
  ExprStmt(const Position &posBeg, const Position &posEnd,
           std::shared_ptr<Expression> expr)
      : Statement(posBeg, posEnd), expr(std::move(expr)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Expression> expr;
};

struct ReturnStmt : Statement {
  ReturnStmt(const Position &posBeg, const Position &posEnd,
             std::shared_ptr<Expression> expr)
      : Statement(posBeg, posEnd), expr(std::move(expr)) {}
  MOCKER_ACCEPT
  std::shared_ptr<Expression> expr;
};

struct ContinueStmt : Statement {
  ContinueStmt(const Position &posBeg, const Position &posEnd)
      : Statement(posBeg, posEnd) {}
  MOCKER_ACCEPT
};

struct BreakStmt : Statement {
  BreakStmt(const Position &posBeg, const Position &posEnd)
      : Statement(posBeg, posEnd) {}
  MOCKER_ACCEPT
};

struct CompoundStmt : Statement {
  CompoundStmt(const Position &posBeg, const Position &posEnd,
               std::vector<std::shared_ptr<Statement>> stmts)
      : Statement(posBeg, posEnd), stmts(std::move(stmts)) {}

  MOCKER_ACCEPT

  std::vector<std::shared_ptr<Statement>> stmts;
};

struct IfStmt : Statement {
  IfStmt(const Position &posBeg, const Position &posEnd,
         std::shared_ptr<Expression> condition, std::shared_ptr<Statement> then,
         std::shared_ptr<Statement> else_)
      : Statement(posBeg, posEnd), condition(std::move(condition)),
        then(std::move(then)), else_(std::move(else_)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Expression> condition;
  std::shared_ptr<Statement> then;
  std::shared_ptr<Statement> else_;
};

struct WhileStmt : Statement {
  WhileStmt(const Position &posBeg, const Position &posEnd,
            std::shared_ptr<Expression> condition,
            std::shared_ptr<Statement> body)
      : Statement(posBeg, posEnd), condition(std::move(condition)),
        body(std::move(body)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Expression> condition;
  std::shared_ptr<Statement> body;
};

struct ForStmt : Statement {
  ForStmt(const Position &posBeg, const Position &posEnd,
          std::shared_ptr<Expression> init,
          std::shared_ptr<Expression> condition,
          std::shared_ptr<Expression> update, std::shared_ptr<Statement> body)
      : Statement(posBeg, posEnd), init(std::move(init)),
        condition(std::move(condition)), update(std::move(update)),
        body(std::move(body)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Expression> init, condition, update;
  std::shared_ptr<Statement> body;
};

struct EmptyStmt : Statement {
  EmptyStmt(const Position &posBeg, const Position &posEnd)
      : Statement(posBeg, posEnd) {}
  MOCKER_ACCEPT
};

/*- declaration --------------------------------------------------------------*/

struct Declaration : ASTNode {
  Declaration(const Position &posBeg, const Position &posEnd)
      : ASTNode(posBeg, posEnd) {}

  MOCKER_PURE_ACCEPT
};

struct VarDecl : Declaration {
  VarDecl(const Position &posBeg, const Position &posEnd,
          std::shared_ptr<VarDeclStmt> decl)
      : Declaration(posBeg, posEnd), decl(std::move(decl)) {}

  MOCKER_ACCEPT

  std::shared_ptr<VarDeclStmt> decl;
};

struct FuncDecl : Declaration {
  FuncDecl(
      const Position &posBeg, const Position &posEnd,
      std::shared_ptr<Type> retType, std::shared_ptr<Identifier> identifier,
      std::vector<std::pair<std::shared_ptr<Type>, std::shared_ptr<Identifier>>>
          formalParameters,
      std::shared_ptr<CompoundStmt> body)
      : Declaration(posBeg, posEnd), retType(std::move(retType)),
        identifier(std::move(identifier)),
        formalParameters(std::move(formalParameters)), body(std::move(body)) {}
  MOCKER_ACCEPT

  std::shared_ptr<Type> retType;
  std::shared_ptr<Identifier> identifier;
  std::vector<std::pair<std::shared_ptr<Type>, std::shared_ptr<Identifier>>>
      formalParameters;
  std::shared_ptr<CompoundStmt> body;
};

struct ClassDecl : Declaration {
  ClassDecl(const Position &posBeg, const Position &posEnd,
            std::shared_ptr<Identifier> identifier,
            std::vector<std::shared_ptr<Declaration>> members)
      : Declaration(posBeg, posEnd), identifier(std::move(identifier)),
        members(std::move(members)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Identifier> identifier;
  std::vector<std::shared_ptr<Declaration>> members;

  // attributes
  ScopeID scopeIntroduced;
};

/*- root ---------------------------------------------------------------------*/
struct ASTRoot : ASTNode {
  ASTRoot(const Position &posBeg, const Position &posEnd,
          std::vector<std::shared_ptr<Declaration>> decls)
      : ASTNode(posBeg, posEnd), decls(std::move(decls)) {}

  MOCKER_ACCEPT
  std::vector<std::shared_ptr<Declaration>> decls;
};

} // namespace ast
} // namespace mocker

#endif // MOCKER_AST_H
