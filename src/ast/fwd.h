#ifndef MOCKER_FWD_H
#define MOCKER_FWD_H

namespace mocker {
namespace ast {

class Visitor;
class ConstVisitor;

struct ASTNode;

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

} // namespace ast
} // namespace mocker

#endif // MOCKER_FWD_H
