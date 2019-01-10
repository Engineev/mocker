#include "printer.h"

#include <cassert>

#include "support/small_map.h"

namespace mocker {
namespace ir {

std::string fmtAddr(const std::shared_ptr<Addr> &addr) {
  if (auto p = std::dynamic_pointer_cast<IntLiteral>(addr))
    return std::to_string(p->val);
  if (auto p = std::dynamic_pointer_cast<LocalReg>(addr))
    return "%" + p->identifier;
  if (auto p = std::dynamic_pointer_cast<GlobalReg>(addr))
    return p->identifier;
  if (auto p = std::dynamic_pointer_cast<Label>(addr))
    return "<" + std::to_string(p->id) + ">";
  assert(false);
}

std::string fmtInst(const std::shared_ptr<IRInst> &inst) {
  using namespace std::string_literals;

  if (auto p = std::dynamic_pointer_cast<ArithUnaryInst>(inst)) {
    auto res = fmtAddr(p->dest) + " = ";
    res += (p->op == ArithUnaryInst::Neg ? "neg" : "bitnot");
    res += " " + fmtAddr(p->operand);
    return res;
  }
  if (auto p = std::dynamic_pointer_cast<Alloca>(inst)) {
    return fmtAddr(p->dest) + " = alloca " + std::to_string(p->size);
  }
  if (auto p = std::dynamic_pointer_cast<Malloc>(inst)) {
    return fmtAddr(p->dest) + " = malloc " + fmtAddr(p->size);
  }
  if (auto p = std::dynamic_pointer_cast<Store>(inst)) {
    return "store " + fmtAddr(p->dest) + " " + fmtAddr(p->operand);
  }
  if (auto p = std::dynamic_pointer_cast<Load>(inst)) {
    return fmtAddr(p->dest) + " = load " + fmtAddr(p->addr);
  }
  if (auto p = std::dynamic_pointer_cast<Branch>(inst)) {
    return "br " + fmtAddr(p->condition) + " " + fmtAddr(p->then) + " " +
           fmtAddr(p->else_);
  }
  if (auto p = std::dynamic_pointer_cast<Jump>(inst)) {
    return "jump " + fmtAddr(p->dest);
  }
  if (auto p = std::dynamic_pointer_cast<Ret>(inst)) {
    return "ret " + (p->val ? fmtAddr(p->val) : "void"s);
  }
  if (auto p = std::dynamic_pointer_cast<ArithBinaryInst>(inst)) {
    static SmallMap<ArithBinaryInst::OpType, std::string> opName{
        {ArithBinaryInst::BitOr, "bitor"s},
        {ArithBinaryInst::BitAnd, "bitand"s},
        {ArithBinaryInst::Xor, "xor"s},
        {ArithBinaryInst::Shl, "shl"s},
        {ArithBinaryInst::Shr, "shr"s},
        {ArithBinaryInst::Add, "add"s},
        {ArithBinaryInst::Sub, "sub"s},
        {ArithBinaryInst::Mul, "mul"s},
        {ArithBinaryInst::Div, "div"s},
        {ArithBinaryInst::Mod, "mod"s},
    };
    return fmtAddr(p->dest) + " = " + opName.at(p->op) + " " + fmtAddr(p->lhs) +
           " " + fmtAddr(p->rhs);
  }
  if (auto p = std::dynamic_pointer_cast<Comment>(inst)) {
    return "; " + p->str;
  }
  if (auto p = std::dynamic_pointer_cast<AttachedComment>(inst)) {
    return "; " + p->str;
  }
  if (auto p = std::dynamic_pointer_cast<Call>(inst)) {
    std::string res;
    if (p->dest)
      res = fmtAddr(p->dest) + " = ";
    res += "call " + p->funcName;
    for (auto &addr : p->args)
      res += " " + fmtAddr(addr);
    return res;
  }
  if (auto p = std::dynamic_pointer_cast<Phi>(inst)) {
    std::string res = fmtAddr(p->dest) + " = phi ";
    for (auto &kv : p->options)
      res += "[" + fmtAddr(kv.first) + ", " + fmtAddr(kv.second) + "] ";
    return res;
  }
  if (auto p = std::dynamic_pointer_cast<RelationInst>(inst)) {
    static SmallMap<RelationInst::OpType, std::string> opName{
        {RelationInst::OpType::Ne, "ne"s}, {RelationInst::OpType::Eq, "eq"s},
        {RelationInst::OpType::Lt, "lt"s}, {RelationInst::OpType::Le, "le"s},
        {RelationInst::OpType::Gt, "gt"s}, {RelationInst::OpType::Ge, "ge"s}};
    return fmtAddr(p->dest) + " = " + opName.at(p->op) + " " + fmtAddr(p->lhs) +
           " " + fmtAddr(p->rhs);
  }
  assert(false);
}

void Printer::operator()(bool printExternal) const {
  for (auto &var : module.globalVars) {
    std::cout << var.getIdentifier() << "[" << var.getSize() << "]";
    std::cout << " = r\"" << var.getData() << "\"\n";
  }
  std::cout << std::endl;

  for (auto &kv : module.funcs) {
    if (printExternal || !kv.second.isExternalFunc()) {
      printFunc(kv.first, kv.second);
      std::cout << std::endl;
    }
  }
}

void Printer::printFunc(const std::string &name,
                        const FunctionModule &func) const {
  std::string attachedComment;

  std::cout << "define " << name << "(";
  for (std::size_t i = 0; i < func.getArgs().size(); ++i) {
    if (i != 0)
      std::cout << ", ";
    std::cout << func.getArgs()[i];
  }
  std::cout << ")";
  if (func.isExternalFunc()) {
    std::cout << " external\n";
    return;
  }
  std::cout << " {\n";
  for (const auto &bb : func.getBBs()) {
    std::cout << "<" << bb.getLabelID() << ">:\n";
    for (const auto &inst : bb.getInsts()) {
      if (auto p = std::dynamic_pointer_cast<AttachedComment>(inst)) {
        attachedComment = p->str;
        continue;
      }

      auto str = fmtInst(inst) +
                 (attachedComment.empty() ? "" : "  ; " + attachedComment);
      attachedComment.clear();
      if (str.at(0) != ';')
        std::cout << "  ";
      std::cout << str << "\n";
    }
  }
  std::cout << "}" << std::endl;
}
} // namespace ir
} // namespace mocker
