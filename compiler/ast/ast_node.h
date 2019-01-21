#ifndef MOCKER_AST_H
#define MOCKER_AST_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/position.h"
#include "fwd.h"
#include "visitor.h"

#define MOCKER_PURE_ACCEPT                                                     \
  void accept(const Visitor &vis) override = 0;                                \
  void accept(const ConstVisitor &vis) const override = 0;

#define MOCKER_ACCEPT                                                          \
  void accept(const Visitor &vis) override { vis(*this); }                     \
  void accept(const ConstVisitor &vis) const override { vis(*this); }

namespace mocker {
namespace ast {

using NodeID = std::uintptr_t;

struct ASTNode : public std::enable_shared_from_this<ASTNode> {
  virtual ~ASTNode() = default;

  NodeID getID() const { return (NodeID)this; }

  virtual void accept(const Visitor &vis) = 0;
  virtual void accept(const ConstVisitor &vis) const = 0;
};

/*- misc ---------------------------------------------------------------------*/

struct Identifier : ASTNode {
  explicit Identifier(std::string val) : val(std::move(val)) {}

  MOCKER_ACCEPT
  std::string val;
};

struct Type : ASTNode {
  MOCKER_PURE_ACCEPT
};

struct NonarrayType : Type {
  MOCKER_PURE_ACCEPT
};

struct BuiltinType : NonarrayType {
  enum Type { Null, Int, String, Bool };

  BuiltinType() = default;
  explicit BuiltinType(Type type) : type(type) {}

  MOCKER_ACCEPT

  Type type = Null;
};

struct UserDefinedType : NonarrayType {
  UserDefinedType() = default;
  explicit UserDefinedType(std::shared_ptr<Identifier> name)
      : name(std::move(name)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Identifier> name;
};

struct ArrayType : Type {
  ArrayType() = default;
  explicit ArrayType(std::shared_ptr<Type> baseType)
      : baseType(std::move(baseType)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Type> baseType;
};

/*- expression ---------------------------------------------------------------*/

struct Expression : ASTNode {
  MOCKER_PURE_ACCEPT
};

struct LiteralExpr : Expression {
  MOCKER_PURE_ACCEPT
};

struct IntLitExpr : LiteralExpr {
  IntLitExpr() = default;
  explicit IntLitExpr(Integer val) : val(val) {}

  MOCKER_ACCEPT

  Integer val = 0;
};

struct StringLitExpr : LiteralExpr {
  StringLitExpr() = default;
  explicit StringLitExpr(std::string val) : val(std::move(val)) {}

  MOCKER_ACCEPT

  std::string val;
};

struct NullLitExpr : LiteralExpr {
  NullLitExpr() = default;

  MOCKER_ACCEPT
};

struct BoolLitExpr : LiteralExpr {
  BoolLitExpr() = default;
  explicit BoolLitExpr(bool val) : val(val) {}

  MOCKER_ACCEPT

  bool val = false;
};

struct IdentifierExpr : Expression {
  explicit IdentifierExpr(std::shared_ptr<Identifier> identifier)
      : identifier(std::move(identifier)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Identifier> identifier;
};

struct UnaryExpr : Expression {
  enum OpType { PreInc, PostInc, PreDec, PostDec, Neg, LogicalNot, BitNot };

  UnaryExpr(OpType op, std::shared_ptr<Expression> operand)
      : op(op), operand(std::move(operand)) {}

  MOCKER_ACCEPT

  OpType op;
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

  BinaryExpr(OpType op, std::shared_ptr<Expression> lhs,
             std::shared_ptr<Expression> rhs)
      : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

  MOCKER_ACCEPT

  OpType op = Assign;
  std::shared_ptr<Expression> lhs, rhs;
};

struct FuncCallExpr : Expression {
  FuncCallExpr(std::shared_ptr<Expression> instance,
               std::shared_ptr<Identifier> identifier,
               std::vector<std::shared_ptr<Expression>> args)
      : instance(std::move(instance)), identifier(std::move(identifier)),
        args(std::move(args)) {}

  MOCKER_ACCEPT

  // e.g. In a[0].foo(1, 2), [instance] is a[0], [identifier] is foo and [args]
  // is 1, 2. If it is a free function call, then [instance] is empty.
  std::shared_ptr<Expression> instance;
  std::shared_ptr<Identifier> identifier;
  std::vector<std::shared_ptr<Expression>> args;
};

struct NewExpr : Expression {
  NewExpr(std::shared_ptr<Type> type,
          std::vector<std::shared_ptr<Expression>> providedDims)
      : type(std::move(type)), providedDims(std::move(providedDims)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Type> type;
  std::vector<std::shared_ptr<Expression>> providedDims;
};

/*- statement ----------------------------------------------------------------*/

struct Statement : ASTNode {
  MOCKER_PURE_ACCEPT
};

struct VarDeclStmt : Statement {
  VarDeclStmt(std::shared_ptr<Type> type,
              std::shared_ptr<Identifier> identifier,
              std::shared_ptr<Expression> initExpr = nullptr)
      : type(std::move(type)), identifier(std::move(identifier)),
        initExpr(std::move(initExpr)) {}
  MOCKER_ACCEPT

  std::shared_ptr<Type> type;
  std::shared_ptr<Identifier> identifier;
  std::shared_ptr<Expression> initExpr;
};

struct ExprStmt : Statement {
  explicit ExprStmt(std::shared_ptr<Expression> expr) : expr(std::move(expr)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Expression> expr;
};

struct ReturnStmt : Statement {
  explicit ReturnStmt(std::shared_ptr<Expression> expr)
      : expr(std::move(expr)) {}
  MOCKER_ACCEPT
  std::shared_ptr<Expression> expr;
};

struct ContinueStmt : Statement {
  MOCKER_ACCEPT
};

struct BreakStmt : Statement {
  MOCKER_ACCEPT
};

struct CompoundStmt : Statement {
  explicit CompoundStmt(std::vector<std::shared_ptr<Statement>> stmts)
      : stmts(std::move(stmts)) {}

  MOCKER_ACCEPT

  std::vector<std::shared_ptr<Statement>> stmts;
};

struct IfStmt : Statement {
  IfStmt(std::shared_ptr<Expression> condition, std::shared_ptr<Statement> then,
         std::shared_ptr<Statement> else_)
      : condition(std::move(condition)), then(std::move(then)),
        else_(std::move(else_)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Expression> condition;
  std::shared_ptr<Statement> then;
  std::shared_ptr<Statement> else_;
};

struct WhileStmt : Statement {
  WhileStmt(std::shared_ptr<Expression> condition,
            std::shared_ptr<Statement> body)
      : condition(std::move(condition)), body(std::move(body)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Expression> condition;
  std::shared_ptr<Statement> body;
};

struct ForStmt : Statement {
  ForStmt(std::shared_ptr<Expression> init,
          std::shared_ptr<Expression> condition,
          std::shared_ptr<Expression> update, std::shared_ptr<Statement> body)
      : init(std::move(init)), condition(std::move(condition)),
        update(std::move(update)), body(std::move(body)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Expression> init, condition, update;
  std::shared_ptr<Statement> body;
};

struct EmptyStmt : Statement {
  MOCKER_ACCEPT
};

/*- declaration --------------------------------------------------------------*/

struct Declaration : ASTNode {
  MOCKER_PURE_ACCEPT
};

struct VarDecl : Declaration {
  explicit VarDecl(std::shared_ptr<VarDeclStmt> decl) : decl(std::move(decl)) {}

  MOCKER_ACCEPT

  std::shared_ptr<VarDeclStmt> decl;
};

struct FuncDecl : Declaration {
  FuncDecl(std::shared_ptr<Type> retType,
           std::shared_ptr<Identifier> identifier,
           std::vector<std::shared_ptr<VarDeclStmt>> formalParameters,
           std::shared_ptr<CompoundStmt> body)
      : retType(std::move(retType)), identifier(std::move(identifier)),
        formalParameters(std::move(formalParameters)), body(std::move(body)) {}
  MOCKER_ACCEPT

  std::shared_ptr<Type> retType;
  std::shared_ptr<Identifier> identifier;
  std::vector<std::shared_ptr<VarDeclStmt>> formalParameters;
  std::shared_ptr<CompoundStmt> body;
};

struct ClassDecl : Declaration {
  ClassDecl(std::shared_ptr<Identifier> identifier,
            std::vector<std::shared_ptr<Declaration>> members)
      : identifier(std::move(identifier)), members(std::move(members)) {}

  MOCKER_ACCEPT

  std::shared_ptr<Identifier> identifier;
  std::vector<std::shared_ptr<Declaration>> members;
};

/*- root ---------------------------------------------------------------------*/
struct ASTRoot : ASTNode {
  explicit ASTRoot(std::vector<std::shared_ptr<Declaration>> decls)
      : decls(std::move(decls)) {}

  MOCKER_ACCEPT
  std::vector<std::shared_ptr<Declaration>> decls;
};

} // namespace ast
} // namespace mocker

#endif // MOCKER_AST_H
