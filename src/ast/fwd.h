#ifndef MOCKER_FWD_H
#define MOCKER_FWD_H

namespace mocker {
namespace ast {

class Visitor;
class ConstVisitor;

struct ASTNode;

struct Identifier;

struct Type;
struct NonarrayType;
struct BuiltinType;
struct UserDefinedType;
struct ArrayType;

struct Expression;
struct LiteralExpr;
struct IntLitExpr;
struct StringLitExpr;
struct NullLitExpr;
struct BoolLitExpr;
struct IdentifierExpr;
struct UnaryExpr;
struct BinaryExpr;
struct FuncCallExpr;
struct NewExpr;

struct Statement;
struct VarDeclStmt;
struct ExprStmt;
struct ReturnStmt;
struct ContinueStmt;
struct BreakStmt;
struct CompoundStmt;
struct IfStmt;
struct WhileStmt;
struct ForStmt;
struct EmptyStmt;

struct Declaration;
struct VarDecl;
struct FuncDecl;
struct ClassDecl;

struct ASTRoot;

} // namespace ast
} // namespace mocker

#endif // MOCKER_FWD_H
