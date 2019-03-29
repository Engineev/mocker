#include "printer.h"

#include <cassert>
#include <sstream>

#include "helper.h"
#include "small_map.h"

namespace mocker {
namespace ir {

std::string fmtAddr(const std::shared_ptr<Addr> &addr) {
  if (auto p = dyc<IntLiteral>(addr))
    return std::to_string(p->getVal());
  if (auto p = dyc<LocalReg>(addr))
    return "%" + p->getIdentifier();
  if (auto p = dyc<GlobalReg>(addr))
    return p->getIdentifier();
  if (auto p = dyc<Label>(addr))
    return "<" + std::to_string(p->getID()) + ">";
  assert(false);
}

std::string fmtInst(const std::shared_ptr<IRInst> &inst) {
  using namespace std::string_literals;

  if (dyc<Deleted>(inst)) {
    return "";
  }
  if (auto p = dyc<Assign>(inst)) {
    return fmtAddr(p->getDest()) + " = assign " + fmtAddr(p->getOperand());
  }
  if (auto p = dyc<ArithUnaryInst>(inst)) {
    auto res = fmtAddr(p->getDest()) + " = ";
    res += (p->getOp() == ArithUnaryInst::Neg ? "neg" : "bitnot");
    res += " " + fmtAddr(p->getOperand());
    return res;
  }
  if (auto p = dyc<AllocVar>(inst)) {
    return fmtAddr(p->getDest()) + " = allocvar";
  }
  if (auto p = dyc<Malloc>(inst)) {
    return fmtAddr(p->getDest()) + " = malloc " + fmtAddr(p->getSize());
  }
  if (auto p = dyc<Store>(inst)) {
    return "store " + fmtAddr(p->getAddr()) + " " + fmtAddr(p->getVal());
  }
  if (auto p = dyc<Load>(inst)) {
    return fmtAddr(p->getDest()) + " = load " + fmtAddr(p->getAddr());
  }
  if (auto p = dyc<Branch>(inst)) {
    return "br " + fmtAddr(p->getCondition()) + " " + fmtAddr(p->getThen()) +
           " " + fmtAddr(p->getElse());
  }
  if (auto p = dyc<Jump>(inst)) {
    return "jump " + fmtAddr(p->getLabel());
  }
  if (auto p = dyc<Ret>(inst)) {
    return "ret " + (p->getVal() ? fmtAddr(p->getVal()) : "void"s);
  }
  if (auto p = dyc<ArithBinaryInst>(inst)) {
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
    return fmtAddr(p->getDest()) + " = " + opName.at(p->getOp()) + " " +
           fmtAddr(p->getLhs()) + " " + fmtAddr(p->getRhs());
  }
  if (auto p = dyc<Comment>(inst)) {
    return "; " + p->getContent();
  }
  if (auto p = dyc<AttachedComment>(inst)) {
    return "; " + p->getContent();
  }
  if (auto p = dyc<Call>(inst)) {
    std::string res;
    if (p->getDest())
      res = fmtAddr(p->getDest()) + " = ";
    res += "call " + p->getFuncName();
    for (auto &addr : p->getArgs())
      res += " " + fmtAddr(addr);
    return res;
  }
  if (auto p = dyc<StrCpy>(inst)) {
    std::string fmtStr;
    for (auto &ch : p->getData()) {
      if (ch == '\n') {
        fmtStr += "\\n";
        continue;
      }
      if (ch == '\\') {
        fmtStr += "\\\\";
        continue;
      }
      if (ch == '\"') {
        fmtStr += "\\\"";
        continue;
      }
      fmtStr.push_back(ch);
    }
    return "strcpy " + fmtAddr(p->getAddr()) + " \"" + fmtStr + "\"";
  }
  if (auto p = dyc<Phi>(inst)) {
    std::string res = fmtAddr(p->getDest()) + " = phi ";
    for (auto &kv : p->getOptions())
      res += "[ " + fmtAddr(kv.first) + " " + fmtAddr(kv.second) + " ] ";
    return res;
  }
  if (auto p = dyc<RelationInst>(inst)) {
    static SmallMap<RelationInst::OpType, std::string> opName{
        {RelationInst::OpType::Ne, "ne"s}, {RelationInst::OpType::Eq, "eq"s},
        {RelationInst::OpType::Lt, "lt"s}, {RelationInst::OpType::Le, "le"s},
        {RelationInst::OpType::Gt, "gt"s}, {RelationInst::OpType::Ge, "ge"s}};
    return fmtAddr(p->getDest()) + " = " + opName.at(p->getOp()) + " " +
           fmtAddr(p->getLhs()) + " " + fmtAddr(p->getRhs());
  }
  assert(false);
}

void printFunc(const FunctionModule &func, std::ostream &out) {
  std::string attachedComment;

  out << "define " << func.getIdentifier() << " ( ";
  for (std::size_t i = 0; i < func.getArgs().size(); ++i) {
    if (i != 0)
      out << " ";
    out << func.getArgs()[i];
  }
  out << " )";
  if (func.isExternalFunc()) {
    out << " external\n";
    return;
  }
  out << " {\n";
  for (const auto &bb : func.getBBs()) {
    out << "<" << bb.getLabelID() << ">:\n";
    for (const auto &inst : bb.getInsts()) {
      if (auto p = dyc<AttachedComment>(inst)) {
        attachedComment = p->getContent();
        continue;
      }

      auto str = fmtInst(inst) +
                 (attachedComment.empty() ? "" : "  ; " + attachedComment);
      attachedComment.clear();
      if (str.at(0) != ';')
        out << "  ";
      out << str << "\n";
    }
  }
  out << "}" << std::endl;
}

void printModule(const Module &module, std::ostream &out) {
  for (auto &var : module.getGlobalVars())
    out << fmtInst(var) << std::endl;
  out << std::endl;

  for (auto &kv : module.getFuncs()) {
    printFunc(kv.second, out);
    out << std::endl;
  }
}

} // namespace ir
} // namespace mocker
