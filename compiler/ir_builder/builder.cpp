#include "builder.h"

#include <functional>

#include "defer.h"
#include "small_map.h"

namespace mocker {
namespace ir {

void Builder::operator()(const ast::IntLitExpr &node) const {
  ctx.setExprAddr(node.getID(), std::make_shared<IntLiteral>(node.val));
  ctx.markExprTrivial(node);
}

void Builder::operator()(const ast::BoolLitExpr &node) const {
  ctx.setExprAddr(node.getID(),
                  std::make_shared<IntLiteral>((Integer)node.val));
  ctx.markExprTrivial(node);
  ctx.checkLogicalExpr(node);
}

void Builder::operator()(const ast::StringLitExpr &node) const {
  auto globalReg = ctx.addStringLiteral(node.val);
  auto res = ctx.makeTempLocalReg();
  ctx.emplaceInst<Load>(res, globalReg);
  ctx.setExprAddr(node.getID(), res);
}

void Builder::operator()(const ast::NullLitExpr &node) const {
  ctx.setExprAddr(node.getID(), makeReg("@null"));
}

void Builder::operator()(const ast::IdentifierExpr &node) const {
  auto dest = ctx.makeTempLocalReg();
  ctx.setExprAddr(node.getID(), dest);
  ctx.emplaceInst<Load>(dest, makeReg(node.identifier->val));
  ctx.markExprTrivial(node);
  ctx.checkLogicalExpr(node);
}

void Builder::operator()(const ast::UnaryExpr &node) const {
  Defer defer([&node, this] {
    if (ctx.isTrivial(node.operand))
      ctx.markExprTrivial(node);
  });

  // arithmetic
  if (node.op == ast::UnaryExpr::BitNot || node.op == ast::UnaryExpr::Neg) {
    auto bak = ctx.getLogicalExprInfo().empty;
    ctx.getLogicalExprInfo().empty = true;
    visit(*node.operand);
    ctx.getLogicalExprInfo().empty = bak;
    ArithUnaryInst::OpType op =
        (node.op == ast::UnaryExpr::BitNot ? ArithUnaryInst::OpType::BitNot
                                           : ArithUnaryInst::OpType::Neg);
    auto dest = ctx.makeTempLocalReg();
    ctx.setExprAddr(node.getID(), dest);
    auto operand = ctx.getExprAddr(node.operand->getID());
    ctx.emplaceInst<ArithUnaryInst>(dest, op, operand);
    return;
  }
  if (node.op == ast::UnaryExpr::PreInc || node.op == ast::UnaryExpr::PostInc ||
      node.op == ast::UnaryExpr::PreDec || node.op == ast::UnaryExpr::PostDec) {
    ArithBinaryInst::OpType op =
        (node.op == ast::UnaryExpr::PreInc || node.op == ast::UnaryExpr::PostInc
             ? ArithBinaryInst::OpType::Add
             : ArithBinaryInst::OpType::Sub);
    bool isPre =
        node.op == ast::UnaryExpr::PreInc || node.op == ast::UnaryExpr::PreDec;

    auto bak = ctx.getLogicalExprInfo().empty;
    ctx.getLogicalExprInfo().empty = true;
    auto addr = getElementPtr(node.operand);
    ctx.getLogicalExprInfo().empty = bak;
    auto oldVal = ctx.makeTempLocalReg();
    ctx.emplaceInst<Load>(oldVal, addr);

    auto newVal = ctx.makeTempLocalReg();
    auto lit1 = std::make_shared<IntLiteral>((Integer)1);
    ctx.emplaceInst<ArithBinaryInst>(newVal, op, oldVal, lit1);

    ctx.setExprAddr(node.getID(), isPre ? newVal : oldVal);
    ctx.emplaceInst<Store>(addr, newVal);

    return;
  }

  assert(node.op == ast::UnaryExpr::LogicalNot);
  if (!ctx.getLogicalExprInfo().empty)
    std::swap(ctx.getLogicalExprInfo().trueNext,
              ctx.getLogicalExprInfo().falseNext);
  visit(*node.operand);
  auto dest = ctx.makeTempLocalReg();
  ctx.setExprAddr(node.getID(), dest);
  if (ctx.getLogicalExprInfo().empty) {
    auto operand = ctx.getExprAddr(node.operand->getID());
    ctx.emplaceInst<ArithBinaryInst>(dest, ArithBinaryInst::Xor, operand,
                                     std::make_shared<IntLiteral>(1));
  }
}

void Builder::operator()(const ast::BinaryExpr &node) const {
  using namespace std::string_literals;
  if (node.op == ast::BinaryExpr::Assign) {
    visit(*node.rhs);
    auto rhsVal = ctx.getExprAddr(node.rhs->getID());
    auto lhsAddr = getElementPtr(node.lhs);
    ctx.emplaceInst<Store>(lhsAddr, rhsVal);
    return;
  }
  if (node.op == ast::BinaryExpr::Subscript ||
      node.op == ast::BinaryExpr::Member) {
    auto bak = ctx.getLogicalExprInfo().empty;
    ctx.getLogicalExprInfo().empty = true;
    auto addr = getElementPtr(std::static_pointer_cast<ast::Expression>(
        const_cast<ast::BinaryExpr &>(node).shared_from_this()));
    ctx.getLogicalExprInfo().empty = bak;
    auto val = ctx.makeTempLocalReg("valOrInstPtr");
    ctx.emplaceInst<Load>(val, addr);
    ctx.setExprAddr(node.getID(), val);
    ctx.checkLogicalExpr(node);
    return;
  }

  Defer defer([&node, this] {
    if (ctx.isTrivial(node.lhs) && ctx.isTrivial(node.rhs))
      ctx.markExprTrivial(node);
  });

  // Eq, Ne, Lt, Gt, Le, Ge,
  if (ast::BinaryExpr::Eq <= node.op && node.op <= ast::BinaryExpr::Ge) {
    auto op = (RelationInst::OpType)(RelationInst::Eq +
                                     (node.op - ast::BinaryExpr::Eq));
    auto val = ctx.makeTempLocalReg("resV");
    ctx.setExprAddr(node.getID(), val);

    auto bak = ctx.getLogicalExprInfo().empty;
    ctx.getLogicalExprInfo().empty = true;
    visit(*node.lhs);
    auto lhs = ctx.getExprAddr(node.lhs->getID());
    ctx.getLogicalExprInfo().empty = true;
    visit(*node.rhs);
    ctx.getLogicalExprInfo().empty = bak;
    auto rhs = ctx.getExprAddr(node.rhs->getID());

    if (isStrTy(ctx.getExprType(node.lhs->getID()))) {
      static SmallMap<ast::BinaryExpr::OpType, std::string> opMap{
          {ast::BinaryExpr::Eq, "#string#equal"s},
          {ast::BinaryExpr::Ne, "#string#inequal"},
          {ast::BinaryExpr::Le, "#string#less_equal"},
          {ast::BinaryExpr::Lt, "#string#less"},
          {ast::BinaryExpr::Ge, "#string#less_equal"},
          {ast::BinaryExpr::Gt, "#string#less"},
      };
      if (node.op == ast::BinaryExpr::Ge || node.op == ast::BinaryExpr::Gt) {
        std::swap(lhs, rhs);
      }
      ctx.emplaceInst<Call>(val, opMap.at(node.op), lhs, rhs);
    } else {
      ctx.emplaceInst<RelationInst>(val, op, lhs, rhs);
    }

    ctx.checkLogicalExpr(node);
    return;
  }
  if (node.op == ast::BinaryExpr::LogicalAnd ||
      node.op == ast::BinaryExpr::LogicalOr) {
    translateLogicBinary(node);
    return;
  }
  // string
  if (isStrTy(ctx.getExprType(node.getID()))) {
    auto bak = ctx.getLogicalExprInfo().empty;
    ctx.getLogicalExprInfo().empty = true;
    visit(*node.lhs);
    visit(*node.rhs);
    ctx.getLogicalExprInfo().empty = bak;
    auto res = ctx.makeTempLocalReg("newStr");
    ctx.emplaceInst<Call>(res, "#string#add",
                          ctx.getExprAddr(node.lhs->getID()),
                          ctx.getExprAddr(node.rhs->getID()));
    ctx.setExprAddr(node.getID(), res);
    return;
  }

  static SmallMap<ast::BinaryExpr::OpType, ArithBinaryInst::OpType> opMap{
      {ast::BinaryExpr::BitOr, ArithBinaryInst::BitOr},
      {ast::BinaryExpr::BitAnd, ArithBinaryInst::BitAnd},
      {ast::BinaryExpr::Xor, ArithBinaryInst::Xor},
      {ast::BinaryExpr::Shl, ArithBinaryInst::Shl},
      {ast::BinaryExpr::Shr, ArithBinaryInst::Shr},
      {ast::BinaryExpr::Add, ArithBinaryInst::Add},
      {ast::BinaryExpr::Sub, ArithBinaryInst::Sub},
      {ast::BinaryExpr::Mul, ArithBinaryInst::Mul},
      {ast::BinaryExpr::Div, ArithBinaryInst::Div},
      {ast::BinaryExpr::Mod, ArithBinaryInst::Mod},
  };
  auto bak = ctx.getLogicalExprInfo().empty;
  ctx.getLogicalExprInfo().empty = true;
  visit(*node.lhs);
  visit(*node.rhs);
  ctx.getLogicalExprInfo().empty = bak;
  auto res = ctx.makeTempLocalReg();
  ctx.emplaceInst<ArithBinaryInst>(res, opMap.at(node.op),
                                   ctx.getExprAddr(node.lhs->getID()),
                                   ctx.getExprAddr(node.rhs->getID()));
  ctx.setExprAddr(node.getID(), res);
}

void Builder::operator()(const ast::FuncCallExpr &node) const {
  auto bak = ctx.getLogicalExprInfo().empty;
  ctx.getLogicalExprInfo().empty = true;

  std::string funcName = node.identifier->val;
  if (funcName == "print" || funcName == "println") {
    translatePrint(node);
    return;
  }
  translateCall(node);
  ctx.getLogicalExprInfo().empty = bak;
  ctx.checkLogicalExpr(node);
}

void Builder::operator()(const ast::NewExpr &node) const {
  auto p = std::dynamic_pointer_cast<ast::ArrayType>(node.type);
  if (!p) {
    auto instancePtr = makeNewNonarray(node.type);
    ctx.setExprAddr(node.getID(), instancePtr);
    return;
  }
  std::queue<std::shared_ptr<Addr>> providedSize;
  for (auto &exp : node.providedDims) {
    visit(*exp);
    providedSize.emplace(ctx.getExprAddr(exp->getID()));
  }
  auto res = translateNewArray(node.type, providedSize);
  ctx.setExprAddr(node.getID(), res);
}

void Builder::operator()(const ast::VarDeclStmt &node) const {
  auto &func = ctx.getCurFunc();
  auto name = node.identifier->val;
  auto valPtr = makeReg(name);
  ctx.appendInstFront(func.getFirstBB(), std::make_shared<Alloca>(valPtr));

  if (node.initExpr && !isNullTy(ctx.getExprType(node.initExpr->getID()))) {
    visit(*node.initExpr);
    ctx.emplaceInst<Store>(makeReg(name),
                           ctx.getExprAddr(node.initExpr->getID()));
    return;
  }
  if (isBoolTy(node.type) || isIntTy(node.type)) {
    return;
  }
  ctx.emplaceInst<Store>(makeReg(name), makeReg("@null"));
}

void Builder::operator()(const ast::ExprStmt &node) const {
  if (!ctx.canSkip(node.expr->getID()))
    visit(*node.expr);
}

void Builder::operator()(const ast::ReturnStmt &node) const {
  if (node.expr)
    visit(*node.expr);
  ctx.emplaceInst<Ret>(node.expr ? ctx.getExprAddr(node.expr->getID())
                                 : nullptr);
}

void Builder::operator()(const ast::ContinueStmt &node) const {
  auto originBB = ctx.getCurBasicBlock();
  FunctionModule &func = ctx.getCurFunc();

  ctx.emplaceInst<Jump>(ctx.getCurLoopUpdate());

  auto successorBB = func.insertBBAfter(originBB);
  ctx.setCurBasicBlock(successorBB);
}

void Builder::operator()(const ast::BreakStmt &node) const {
  auto originBB = ctx.getCurBasicBlock();
  FunctionModule &func = ctx.getCurFunc();

  auto loopSuccessor = ctx.getCurLoopSuccessor();
  ctx.emplaceInst<Jump>(loopSuccessor);

  auto successorBB = func.insertBBAfter(originBB);
  ctx.setCurBasicBlock(successorBB);
}

void Builder::operator()(const ast::CompoundStmt &node) const {
  for (auto &stmt : node.stmts) {
    visit(*stmt);
  }
}

void Builder::operator()(const ast::IfStmt &node) const {
  auto originalBB = ctx.getCurBasicBlock();
  FunctionModule &func = ctx.getCurFunc();

  auto thenFirstBB = func.insertBBAfter(originalBB);
  auto thenFirstLabel = getBBLabel(thenFirstBB);
  ctx.setCurBasicBlock(thenFirstBB);
  visit(*node.then);
  auto thenLastBB = ctx.getCurBasicBlock();

  BBLIter elseFirstBB, elseLastBB;
  std::shared_ptr<Label> elseFirstLabel = nullptr;
  if (node.else_) {
    elseFirstBB = func.insertBBAfter(thenLastBB);
    elseFirstLabel = getBBLabel(elseFirstBB);
    ctx.setCurBasicBlock(elseFirstBB);
    visit(*node.else_);
    elseLastBB = ctx.getCurBasicBlock();
  }

  auto successorBB = func.insertBBAfter(node.else_ ? elseLastBB : thenLastBB);
  auto successorLabel = std::make_shared<Label>(successorBB->getLabelID());
  auto jump = std::make_shared<Jump>(successorLabel);
  if (!thenLastBB->isCompleted())
    ctx.appendInst(thenLastBB, jump);
  if (node.else_ && !elseLastBB->isCompleted())
    ctx.appendInst(elseLastBB, jump);

  auto &info = ctx.getLogicalExprInfo();
  info.inCondition = true;
  info.trueNext = thenFirstBB;
  info.falseNext = node.else_ ? elseFirstBB : successorBB;
  info.empty = false;
  ctx.setCurBasicBlock(originalBB);
  visit(*node.condition);
  info.empty = true;
  info.inCondition = false;

  ctx.setCurBasicBlock(successorBB);
}

void Builder::operator()(const ast::WhileStmt &node) const {
  translateLoop(node.condition, node.body);
}

void Builder::operator()(const ast::ForStmt &node) const {
  if (node.init)
    visit(*node.init);
  translateLoop(node.condition, node.body, node.update);
}

void Builder::operator()(const ast::FuncDecl &node) const {
  bool isMember = node.identifier->val.at(0) == '#';
  std::vector<std::string> paramIdent;
  if (isMember)
    paramIdent.emplace_back("this");
  for (auto &param : node.formalParameters)
    paramIdent.emplace_back(param->identifier->val);

  auto &func =
      ctx.addFunc(FunctionModule(node.identifier->val, std::move(paramIdent)));
  ctx.initFuncCtx(node.formalParameters.size());
  ctx.setCurBasicBlock(func.pushBackBB());

  if (isMember) {
    auto reg = makeReg("this");
    ctx.appendInstFront(func.getFirstBB(), std::make_shared<Alloca>(reg));
    ctx.emplaceInst<Store>(reg, makeReg("0"));
  }
  for (std::size_t i = 0; i < node.formalParameters.size(); ++i) {
    auto name = node.formalParameters[i]->identifier->val;
    auto reg = makeReg(name);
    ctx.appendInstFront(func.getFirstBB(), std::make_shared<Alloca>(reg));
    ctx.emplaceInst<Store>(reg, makeReg(std::to_string(i + isMember)));
  }

  for (auto &p : node.body->stmts) {
    if (ctx.getCurBasicBlock()->isCompleted())
      return;
    visit(*p);
  }
  if (!ctx.getCurBasicBlock()->isCompleted())
    ctx.emplaceInst<Ret>();
}

void Builder::operator()(const ast::ClassDecl &node) const {
  for (auto &member : node.members)
    if (auto p = std::dynamic_pointer_cast<ast::FuncDecl>(member))
      visit(*p);
}

void Builder::operator()(const ast::ASTRoot &node) const {
  addBuiltinAndExternal();

  for (auto &decl : node.decls) {
    if (auto p = std::dynamic_pointer_cast<ast::ClassDecl>(decl))
      addClassLayout(*p);
  }

  for (auto &decl : node.decls) {
    if (auto p = std::dynamic_pointer_cast<ast::VarDecl>(decl)) {
      auto type = p->decl->type;
      addGlobalVariable(p);
    }
  }

  for (auto &decl : node.decls) {
    if (!std::dynamic_pointer_cast<ast::VarDecl>(decl))
      visit(*decl);
  }
}

} // namespace ir
} // namespace mocker

namespace mocker {
namespace ir {

void Builder::addClassLayout(const ast::ClassDecl &node) const {
  BuilderContext::ClassLayout classLayout;
  for (auto &mem : node.members) {
    if (auto decl = std::dynamic_pointer_cast<ast::VarDecl>(mem)) {
      std::string varName =
          splitMemberVarIdent(decl->decl->identifier->val).second;
      classLayout.offset[varName] = classLayout.size;
      classLayout.size += 8;
    }
  }
  ctx.addClassLayout(node.identifier->val, std::move(classLayout));
}

std::shared_ptr<Label>
Builder::getBBLabel(BasicBlockList::const_iterator bb) const {
  return std::make_shared<Label>(bb->getLabelID());
}

std::shared_ptr<Addr>
Builder::getElementPtr(const std::shared_ptr<ast::ASTNode> &exp) const {
  // identifier
  if (auto p = std::dynamic_pointer_cast<ast::Identifier>(exp)) {
    return makeReg(p->val);
  }
  // identifier expr
  if (auto p = std::dynamic_pointer_cast<ast::IdentifierExpr>(exp)) {
    return makeReg(p->identifier->val);
  }
  // ++ or --
  if (auto p = std::dynamic_pointer_cast<ast::UnaryExpr>(exp)) {
    assert(p->op == ast::UnaryExpr::PreInc || p->op == ast::UnaryExpr::PreDec);
    return getElementPtr(p->operand);
  }
  auto p = std::dynamic_pointer_cast<ast::BinaryExpr>(exp);
  assert(p);
  visit(*p->lhs);
  // member
  if (p->op == ast::BinaryExpr::Member) {
    auto basePtr = ctx.getExprAddr(p->lhs->getID());
    auto className = std::dynamic_pointer_cast<ast::UserDefinedType>(
                         ctx.getExprType(p->lhs->getID()))
                         ->name->val;
    auto varName =
        std::dynamic_pointer_cast<ast::IdentifierExpr>(p->rhs)->identifier->val;
    auto elementPtr = getMemberElementPtr(basePtr, className, varName);
    return elementPtr;
  }
  // subscript
  if (p->op == ast::BinaryExpr::Subscript) {
    ctx.emplaceInst<Comment>("get element ptr: subscript");
    visit(*p->rhs);

    auto arrayInstPtr = ctx.getExprAddr(p->lhs->getID());
    auto contentPtr = arrayInstPtr;
    auto idx = ctx.getExprAddr(p->rhs->getID());
    auto offset = ctx.makeTempLocalReg("offset");
    ctx.emplaceInst<ArithBinaryInst>(offset, ArithBinaryInst::Mul, idx,
                                     makeILit(8));
    auto addr = ctx.makeTempLocalReg("elementPtr");
    ctx.emplaceInst<ArithBinaryInst>(addr, ArithBinaryInst::Add, contentPtr,
                                     offset);
    ctx.emplaceInst<Comment>("");
    return addr;
  }
  assert(false);
}

std::pair<std::string, std::string>
Builder::splitMemberVarIdent(const std::string &ident) const {
  std::string::const_iterator iter = ident.begin() + 1;
  while (*iter != '#')
    ++iter;
  std::string className(ident.begin() + 1, iter);
  std::string varName(iter + 1, ident.end());
  return {className, varName};
}

void Builder::translateEscapedLogicBinary(
    const mocker::ast::BinaryExpr &node) const {
  auto originBB = ctx.getCurBasicBlock();
  auto originLabel = getBBLabel(originBB);
  FunctionModule &func = ctx.getCurFunc();

  auto boolExpAddr = ctx.makeTempLocalReg("boolExpAddr");
  ctx.appendInstFront(func.getFirstBB(), std::make_shared<Alloca>(boolExpAddr));

  visit(*node.lhs);
  auto lhsVal = ctx.getExprAddr(node.lhs->getID());
  ctx.emplaceInst<Store>(boolExpAddr, lhsVal);
  auto lhsLastBB = ctx.getCurBasicBlock();

  auto rhsFirstBB = func.insertBBAfter(originBB);
  auto rhsFirstLabel = getBBLabel(rhsFirstBB);
  ctx.setCurBasicBlock(rhsFirstBB);
  visit(*node.rhs);
  auto rhsVal = ctx.getExprAddr(node.rhs->getID());
  ctx.emplaceInst<Store>(boolExpAddr, rhsVal);

  auto successorBB = func.pushBackBB();
  auto successorLabel = getBBLabel(successorBB);
  ctx.emplaceInst<Jump>(successorLabel);

  if (!ctx.isTrivial(node.rhs)) {
    if (node.op == ast::BinaryExpr::LogicalAnd) {
      ctx.appendInst(lhsLastBB, std::make_shared<Branch>(
                                    ctx.getExprAddr(node.lhs->getID()),
                                    rhsFirstLabel, successorLabel));
    } else {
      ctx.appendInst(lhsLastBB, std::make_shared<Branch>(
                                    ctx.getExprAddr(node.lhs->getID()),
                                    successorLabel, rhsFirstLabel));
    }

    ctx.setCurBasicBlock(successorBB);
    auto val = ctx.makeTempLocalReg("v");
    ctx.setExprAddr(node.getID(), val);
    ctx.emplaceInst<Load>(val, boolExpAddr);
    return;
  }

  ctx.appendInst(lhsLastBB, std::make_shared<Jump>(rhsFirstLabel));
  ctx.setCurBasicBlock(successorBB);

  rhsVal = ctx.getExprAddr(node.rhs->getID());
  auto val = ctx.makeTempLocalReg("v");
  ctx.setExprAddr(node.getID(), val);
  ctx.emplaceInst<ArithBinaryInst>(val,
                                   node.op == ast::BinaryExpr::LogicalAnd
                                       ? ArithBinaryInst::BitAnd
                                       : ArithBinaryInst::BitOr,
                                   lhsVal, rhsVal);
  ctx.markExprTrivial(node);
}

void Builder::translateLogicBinary(const ast::BinaryExpr &node) const {
  if (!ctx.getLogicalExprInfo().inCondition) {
    translateEscapedLogicBinary(node);
    return;
  }

  auto originBB = ctx.getCurBasicBlock();
  auto originLabel = getBBLabel(originBB);
  FunctionModule &func = ctx.getCurFunc();
  auto lhsFirstBB = originBB;
  auto rhsFirstBB = func.pushBackBB();
  auto &info = ctx.getLogicalExprInfo();

  Defer undo;
  while (info.empty) {
    info.empty = false;

    if (info.inCondition)
      break;

    auto boolExpAddr = ctx.makeTempLocalReg("boolExpAddr");
    ctx.appendInstFront(func.getFirstBB(),
                        std::make_shared<Alloca>(boolExpAddr));

    auto succBB = func.pushBackBB();
    auto succLabel = getBBLabel(succBB);
    auto val = ctx.makeTempLocalReg("v");
    ctx.appendInst(succBB, std::make_shared<Load>(val, boolExpAddr));
    ctx.setExprAddr(node.getID(), val);

    auto jumpSucc = std::make_shared<Jump>(succLabel);
    info.trueNext = func.pushBackBB();
    ctx.appendInst(
        info.trueNext,
        std::make_shared<Store>(boolExpAddr, std::make_shared<IntLiteral>(1)));
    ctx.appendInst(info.trueNext, jumpSucc);
    info.falseNext = func.pushBackBB();
    ctx.appendInst(
        info.falseNext,
        std::make_shared<Store>(boolExpAddr, std::make_shared<IntLiteral>(0)));
    ctx.appendInst(info.falseNext, jumpSucc);

    undo = Defer([this, succBB] {
      ctx.getLogicalExprInfo().empty = true;
      ctx.setCurBasicBlock(succBB);
    });

    break;
  }

  ctx.setCurBasicBlock(lhsFirstBB);
  auto infoBak = info;
  if (node.op == ast::BinaryExpr::LogicalOr) {
    info.trueNext = infoBak.trueNext;
    info.falseNext = rhsFirstBB;
  } else if (node.op == ast::BinaryExpr::LogicalAnd) {
    info.trueNext = rhsFirstBB;
    info.falseNext = infoBak.falseNext;
  }
  visit(*node.lhs);
  info = infoBak;

  ctx.setCurBasicBlock(rhsFirstBB);
  visit(*node.rhs);
}

void Builder::translateLoop(
    const std::shared_ptr<ast::Expression> &condition,
    const std::shared_ptr<ast::Statement> &body,
    const std::shared_ptr<ast::Expression> &update) const {
  ctx.emplaceInst<Comment>("loop");
  auto defer1 = std::shared_ptr<void>(
      nullptr, [&](void *) { ctx.emplaceInst<Comment>(""); });
  auto originBB = ctx.getCurBasicBlock();
  FunctionModule &func = ctx.getCurFunc();

  // insert BBs and get labels
  auto conditionFirstBB = func.insertBBAfter(originBB);
  auto conditionLabel = getBBLabel(conditionFirstBB);
  auto bodyFirstBB = func.insertBBAfter(conditionFirstBB);
  auto bodyLabel = getBBLabel(bodyFirstBB);
  auto successorBB = func.insertBBAfter(bodyFirstBB);
  auto successorLabel = getBBLabel(successorBB);
  auto updateBB = func.pushBackBB();
  auto updateLabel = getBBLabel(updateBB);
  auto defer2 =
      Defer([this, &successorBB] { ctx.setCurBasicBlock(successorBB); });

  auto jump2Cond = std::make_shared<Jump>(conditionLabel);
  ctx.appendInst(originBB, jump2Cond);

  // condition
  ctx.setCurBasicBlock(conditionFirstBB);
  if (condition) {
    auto &info = ctx.getLogicalExprInfo();
    info.inCondition = true;
    info.empty = false;
    info.trueNext = bodyFirstBB;
    info.falseNext = successorBB;
    visit(*condition);
    info.empty = true;
    info.inCondition = false;
  } else {
    auto br = std::make_shared<Branch>(makeILit(1), bodyLabel, successorLabel);
    ctx.appendInst(conditionFirstBB, br);
  }
  ctx.pushLoopEntry(conditionLabel);
  ctx.pushLoopSuccessor(successorLabel);
  ctx.pushLoopUpdate(updateLabel);

  auto pop = std::shared_ptr<void>(nullptr, [this](void *) {
    ctx.popLoopEntry();
    ctx.popLoopSuccessor();
    ctx.popLoopUpdate();
  });

  // body
  ctx.setCurBasicBlock(bodyFirstBB);
  visit(*body);
  auto bodyLastBB = ctx.getCurBasicBlock();
  if (!bodyLastBB->isCompleted())
    ctx.appendInst(std::make_shared<ir::Jump>(updateLabel));

  ctx.setCurBasicBlock(updateBB);
  if (update)
    visit(*update);
  ctx.appendInst(updateBB, jump2Cond);
}

std::shared_ptr<Addr> Builder::translateNewArray(
    const std::shared_ptr<ast::Type> &rawCurType,
    std::queue<std::shared_ptr<Addr>> &sizeProvided) const {
  // We describe the translation of statements "new T[N]" here. First, note that
  // the result is a pointer arrayInstPtr, which points to the address of the
  // first element of the array. (The length of the array is store in front of
  // the array elements.) It is the return value of this function. Hence, the
  // first step is
  //   1. to malloc a piece of memory of length 8 + 8 * N and store the address
  //      + 8 into arrayInstPtr;
  //   2. and store N into the first 8 bytes.
  // Then, we shall construct the array. If T is int or bool, then there is
  // nothing to do. Otherwise, we shall construct each element via a loop. To
  // implement the loop, we first need aa temporary variable to store the
  // address of the current element since our registers are of SSA form, that
  // is, they can not be assigned more than once. Namely, we shall
  //   3. alloca a piece of memory for the temporary varaible and store the
  //      address into the register curElementPtrPtr. And store the address of
  //      the head, namely, the contentPtr, into the address stored in
  //      curElementPtrPtr. (Meanwhile, we shall compute the address of the
  //      end of the array for later use.)
  // After finishing the routine of creating basic blocks and labels, we
  // construct the condition for the loop to end.
  //   4. Load curElementPtr from the address stored in curElementPtrPtr.
  //   5. Compare it with the end pointer and jump according to the result.
  // Now we implement the body of the loop. In fact, what we need to do is just
  // initializing the current element.
  //   6. New an element. (See next section for details.)
  //   7. Store the address into the current element.
  //   8. Update the loop variable and jump back to the condition basic block.
  //
  // In this passage, we handle some details. First, we need to support newing
  // a jagged array. To achieve this, we let this function to be recursive, that
  // is, in step 6., if the type of the element is still array, then, instead of
  // newing an element, we call this function recursively and pass down the
  // remaining provided sizes.
  // Note that the number of sizes provided may be less than the dimension of
  // the array. This case is similar to the int/bool case. We just return after
  // step 2., leaving the elements uninitialized.

  auto &func = ctx.getCurFunc();
  auto curType = std::dynamic_pointer_cast<ast::ArrayType>(rawCurType);
  assert(curType);
  auto elementType = curType->baseType;

  // 1.
  auto size = sizeProvided.front();
  sizeProvided.pop();
  auto memLen = ctx.makeTempLocalReg("memLen");
  ctx.emplaceInst<AttachedComment>("Calculate the memory needed");
  auto contentLen = ctx.makeTempLocalReg("contentLen");
  ctx.emplaceInst<ArithBinaryInst>(contentLen, ArithBinaryInst::Mul,
                                   makeILit(8),
                                   size); // calcMemLen
  ctx.emplaceInst<ArithBinaryInst>(memLen, ArithBinaryInst::Add, contentLen,
                                   makeILit(8));
  auto memPtr = ctx.makeTempLocalReg("memPtr");
  ctx.emplaceInst<Malloc>(memPtr, memLen);

  auto arrayInstPtr = ctx.makeTempLocalReg("arrayInstPtr");
  ctx.emplaceInst<ArithBinaryInst>(arrayInstPtr, ArithBinaryInst::Add, memPtr,
                                   makeILit(8));

  // 2.
  ctx.emplaceInst<Store>(memPtr, size);

  // 3.
  if (isIntTy(elementType) || isBoolTy(elementType))
    return arrayInstPtr;
  if ((bool)std::dynamic_pointer_cast<ast::ArrayType>(elementType) &&
      sizeProvided.empty())
    return arrayInstPtr;

  ctx.emplaceInst<Comment>("init the content of array");
  auto defer = std::shared_ptr<void *>(
      nullptr, [this](void *) { ctx.emplaceInst<Comment>(""); });

  auto curElementPtrPtr = ctx.makeTempLocalReg("curElementPtrPtr");
  ctx.appendInstFront(func.getFirstBB(),
                      std::make_shared<Alloca>(curElementPtrPtr));
  ctx.emplaceInst<Store>(curElementPtrPtr, arrayInstPtr);
  auto contentEndPtr = ctx.makeTempLocalReg("contentEndPtr");
  // calcContentEndPtr
  ctx.emplaceInst<ArithBinaryInst>(contentEndPtr, ArithBinaryInst::Add,
                                   arrayInstPtr, contentLen);

  auto originBB = ctx.getCurBasicBlock();
  auto conditionFirstBB = func.insertBBAfter(originBB);
  auto conditionLabel = getBBLabel(conditionFirstBB);
  auto bodyFirstBB = func.insertBBAfter(conditionFirstBB);
  auto bodyLabel = getBBLabel(bodyFirstBB);
  auto successorBB = func.insertBBAfter(bodyFirstBB);
  auto successorLabel = getBBLabel(successorBB);

  auto jump2condition = std::make_shared<Jump>(conditionLabel);
  ctx.appendInst(jump2condition);

  // 4.
  ctx.setCurBasicBlock(conditionFirstBB);
  auto curElementPtr = ctx.makeTempLocalReg("curElementPtr");
  ctx.emplaceInst<Load>(curElementPtr, curElementPtrPtr);
  // 5.
  auto cmpRes = ctx.makeTempLocalReg("cmpRes");
  ctx.emplaceInst<RelationInst>(cmpRes, RelationInst::Ne, curElementPtr,
                                contentEndPtr);
  ctx.emplaceInst<Branch>(cmpRes, bodyLabel, successorLabel);

  // 6.
  ctx.setCurBasicBlock(bodyFirstBB);
  auto elementInstPtr =
      (bool)std::dynamic_pointer_cast<ast::ArrayType>(elementType)
          ? translateNewArray(elementType, sizeProvided)
          : makeNewNonarray(elementType);
  // 7.
  ctx.emplaceInst<Store>(curElementPtr, elementInstPtr);
  // 8.
  auto nextElementPtr = ctx.makeTempLocalReg("nextElementPtr");
  ctx.emplaceInst<ArithBinaryInst>(nextElementPtr, ArithBinaryInst::Add,
                                   curElementPtr,
                                   makeILit(8)); // calcNextElementPtr
  ctx.emplaceInst<Store>(curElementPtrPtr, nextElementPtr);
  ctx.appendInst(jump2condition);

  ctx.setCurBasicBlock(successorBB);
  return arrayInstPtr;
}

void Builder::addBuiltinAndExternal() const {
  // null
  ctx.addGlobalVar("@null");

  auto add = [this](std::string name, std::vector<std::string> args) {
    ctx.addFunc(FunctionModule(std::move(name), std::move(args), true));
  };

  add("__alloc", {"sz"});

  // extern C
  add("memcpy", {"dest", "src", "count"});

  // builtin functions
  add("print", {"str"});
  add("println", {"str"});
  add("getString", {});
  add("getInt", {});
  add("toString", {"i"});
  add("_printInt", {"x"});
  add("_printlnInt", {"x"});

  // builtin functions of string
  add("#string#_ctor_", {"this"});
  add("#string#length", {"this"});
  add("#string#substring", {"this", "left", "right"});
  add("#string#parseInt", {"this"});
  add("#string#ord", {"this", "pos"});
  add("#string#add", {"lhs", "rhs"});
  add("#string#equal", {"lhs", "rhs"});
  add("#string#inequal", {"lhs", "rhs"});
  add("#string#less_equal", {"lhs", "rhs"});
  add("#string#less", {"lhs", "rhs"});

  // builtin functions of array
  add("#_array_#_ctor_", {"this", "arraySize", "elementSize"});
  add("#_array_#size", {"this"});
}

void Builder::addGlobalVariable(
    const std::shared_ptr<ast::VarDecl> &decl) const {
  auto ident = decl->decl->identifier->val;
  ctx.addGlobalVar(ident);

  if (!decl->decl->initExpr)
    return;

  auto reg = makeReg(ident);
  auto funcName = "_init_" + ident;
  auto &func = ctx.addFunc(FunctionModule(funcName, {}));
  ctx.initFuncCtx(0);
  ctx.setCurBasicBlock(func.pushBackBB());
  visit(*decl->decl->initExpr);
  ctx.emplaceInst<Store>(reg, ctx.getExprAddr(decl->decl->initExpr->getID()));
  ctx.emplaceInst<Ret>();
  ctx.emplaceGlobalInitInst<Call>(funcName);
}

void Builder::translateCall(const ast::FuncCallExpr &node) const {
  std::shared_ptr<Addr> dest = node.identifier->val != "print" &&
                                       node.identifier->val != "println" &&
                                       (bool)ctx.getExprType(node.getID())
                                   ? ctx.makeTempLocalReg()
                                   : nullptr;

  std::string funcName = node.identifier->val;
  std::vector<std::shared_ptr<Addr>> args;
  if (funcName.at(0) == '#') {
    visit(*node.instance);
    args.emplace_back(ctx.getExprAddr(node.instance->getID()));
  }
  for (auto &arg : node.args) {
    visit(*arg);
    args.emplace_back(ctx.getExprAddr(arg->getID()));
  }

  ctx.emplaceInst<Call>(std::static_pointer_cast<ir::Reg>(dest), funcName,
                        args);

  if (dest)
    ctx.setExprAddr(node.getID(), dest);
}

void Builder::translatePrint(const ast::FuncCallExpr &print) const {
  auto val = print.args.at(0);
  bool newline = (print.identifier->val == "println");

  // can be converted to printInt ?
  while (true) {
    auto toString = std::dynamic_pointer_cast<ast::FuncCallExpr>(val);
    if (!toString || toString->identifier->val != "toString")
      break;
    visit(*toString->args.at(0));
    auto arg = ctx.getExprAddr(toString->args.at(0)->getID());
    ctx.emplaceInst<Call>(newline ? "_printlnInt" : "_printInt", arg);
    return;
  }

  // can be split into two print's ?
  while (true) {
    auto concat = std::dynamic_pointer_cast<ast::BinaryExpr>(val);
    if (!concat || concat->op != ast::BinaryExpr::Add)
      break;
    auto printLhs = std::make_shared<ast::FuncCallExpr>(
        nullptr, std::make_shared<ast::Identifier>("print"),
        std::vector<std::shared_ptr<ast::Expression>>{concat->lhs});
    translatePrint(*printLhs);
    auto printRhs = std::make_shared<ast::FuncCallExpr>(
        nullptr,
        std::make_shared<ast::Identifier>(newline ? "println" : "print"),
        std::vector<std::shared_ptr<ast::Expression>>{concat->rhs});
    translatePrint(*printRhs);
    return;
  }

  translateCall(print);
}

std::shared_ptr<Reg>
Builder::getMemberElementPtr(const std::shared_ptr<Addr> &base,
                             const std::string &className,
                             const std::string &varName) const {
  auto attachedComment = std::make_shared<AttachedComment>(
      "getElementPtr: " + className + "::" + varName);
  ctx.appendInst(std::move(attachedComment));

  std::size_t offset = ctx.getOffset(className, varName);
  auto offsetLit = std::make_shared<IntLiteral>((Integer)offset);
  auto varPtr = ctx.makeTempLocalReg("varPtr");
  ctx.emplaceInst<ArithBinaryInst>(varPtr, ArithBinaryInst::Add, base,
                                   offsetLit);
  return varPtr;
}

std::shared_ptr<Reg> Builder::makeReg(std::string identifier) const {
  if (identifier.at(0) == '@') // is global variable
    return std::make_shared<Reg>(std::move(identifier));
  if (identifier.at(0) == '#') { // is member variable
    std::string className, varName;
    std::tie(className, varName) = splitMemberVarIdent(identifier);
    auto instancePtrPtr = std::make_shared<Reg>("this");
    auto instancePtr = ctx.makeTempLocalReg("instPtr");
    ctx.emplaceInst<Load>(instancePtr, instancePtrPtr);
    return getMemberElementPtr(instancePtr, className, varName);
  }
  return std::make_shared<Reg>(std::move(identifier));
}

std::shared_ptr<Addr>
Builder::makeNewNonarray(const std::shared_ptr<ast::Type> &type) const {
  assert(!std::dynamic_pointer_cast<ast::ArrayType>(type));
  if (auto p = std::dynamic_pointer_cast<ast::BuiltinType>(type))
    assert(p->type == ast::BuiltinType::String);

  auto typeSize = makeILit((Integer)getTypeSize(type));
  auto instancePtr = ctx.makeTempLocalReg("instPtr");
  ctx.emplaceInst<Malloc>(instancePtr, typeSize);
  auto ctorName = "#" + getTypeIdentifier(type) + "#_ctor_";
  ctx.emplaceInst<Call>(ctorName, instancePtr);
  return instancePtr;
}

std::shared_ptr<Addr> Builder::makeILit(Integer val) const {
  return std::make_shared<IntLiteral>(val);
}

std::string
Builder::getTypeIdentifier(const std::shared_ptr<ast::Type> &type) const {
  if (auto p = std::dynamic_pointer_cast<ast::BuiltinType>(type)) {
    assert(p->type == ast::BuiltinType::String);
    return "string";
  }
  if (auto p = std::dynamic_pointer_cast<ast::UserDefinedType>(type)) {
    return p->name->val;
  }
  if (auto p = std::dynamic_pointer_cast<ast::ArrayType>(type)) {
    return "_array_";
  }
  assert(false);
}

std::size_t Builder::getTypeSize(const std::shared_ptr<ast::Type> &type) const {
  if (auto p = std::dynamic_pointer_cast<ast::BuiltinType>(type)) {
    if (p->type == ast::BuiltinType::String)
      return 16;
    assert(p->type != ast::BuiltinType::Null);
    return 8;
  }
  if (auto p = std::dynamic_pointer_cast<ast::UserDefinedType>(type)) {
    return ctx.getClassSize(p->name->val);
  }
  if (auto p = std::dynamic_pointer_cast<ast::ArrayType>(type)) {
    return 16;
  }
  assert(false);
}

} // namespace ir
} // namespace mocker