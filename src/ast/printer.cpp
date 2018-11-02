#include "printer.h"

#include "ast_node.h"
#include <string>
#include <unordered_map>

namespace mocker {
namespace ast {

Printer::Printer(std::ostream &out) : out(out), buf(out) {}

void Printer::operator()(const BuiltinType &node) const {
  std::string name;
  switch (node.type) {
  case BuiltinType::Type::Null:
    name = "Null";
    break;
  case BuiltinType::Type::Int:
    name = "Int";
    break;
  case BuiltinType::Type::Bool:
    name = "Bool";
    break;
  case BuiltinType::Type::String:
    name = "String";
    break;
  default:
    assert(false);
  }
  out << "BuiltinType: " + name;
}
void Printer::operator()(const UserDefinedType &node) const {
  out << "UserDefinedType: " + node.name;
}
void Printer::operator()(const ArrayType &node) const {
  out << "ArrayType: {\t\nbaseType = ";
  node.baseType->accept(*this);
  out << "\b\n}";
}

void Printer::operator()(const IntLitExpr &node) const {
  out << "IntLitExpr: val = " << node.val;
}
void Printer::operator()(const BoolLitExpr &node) const {
  out << "BoolLitExpr: val = " << node.val;
}
void Printer::operator()(const StringLitExpr &node) const {
  out << "StringLitExpr: val = " << node.val;
}
void Printer::operator()(const NullLitExpr &node) const {
  out << "NullLitExpr";
}
void Printer::operator()(const IdentifierExpr &node) const {
  out << "IdentifierExpr: identifier = " << node.identifier;
}
void Printer::operator()(const UnaryExpr &node) const {
  std::string opName;
  switch (node.op) {
  case UnaryExpr::OpType::Inc:
    opName = "Inc";
    break;
  case UnaryExpr::OpType::Dec:
    opName = "Dec";
    break;
  case UnaryExpr::OpType::Neg:
    opName = "Neg";
    break;
  case UnaryExpr::OpType::LogicalNot:
    opName = "LogicalNot";
    break;
  case UnaryExpr::OpType::BitNot:
    opName = "BitNot";
    break;
  default:
    assert(false);
  }

  out << "UnaryExpr: {\t\n";
  out << "op = " << opName << '\n';
  out << "operand = ";
  node.operand->accept(*this);
  out << "\b\n}";
}
void Printer::operator()(const BinaryExpr &node) const {
  const std::vector<std::string> names = {
      "Assign", "LogicalOr", "LogicalAnd", "BitOr", "BitAnd", "Xor",
      "Eq",     "Ne",        "Lt",         "Gt",    "Le",     "Ge",
      "Shl",    "Shr",       "Add",        "Sub",   "Mul",    "Div",
      "Mod",    "Subscript", "Member"};
  assert(names[BinaryExpr::OpType::Mul] == "Mul");

  out << "BinaryExpr: {\t\n";
  out << "op = " << names[node.op] << '\n';
  out << "lhs = ";
  node.lhs->accept(*this);
  out << "\nrhs = ";
  node.rhs->accept(*this);
  out << "\b\n}";
}
void Printer::operator()(const FuncCallExpr &node) const {
  out << "FuncCallExpr: {\t\n";
  out << "callee = ";
  node.callee->accept(*this);
  out << "\nargs = [\t";
  for (const auto &arg : node.args) {
    out << "\n";
    arg->accept(*this);
  }
  out << "\b\n]\b\n}";
}
void Printer::operator()(const NewExpr &node) const {
  out << "NewExpr: {\t\n";
  out << "type = ";
  node.type->accept(*this);
  out << "\nprovidedDims = [\t";
  for (const auto &dim : node.providedDims) {
    out << "\n";
    dim->accept(*this);
  }
  out << "\b\n]\b\n}";
}

void print(std::ostream &out, const ASTNode &node) {
  Printer printer(out);
  node.accept(printer);
}

} // namespace ast
} // namespace mocker