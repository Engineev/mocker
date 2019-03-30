#include "builder.h"

#include <functional>

#include "defer.h"
#include "small_map.h"

namespace mocker {
namespace ir {

void Builder::operator()(const ast::IntLitExpr &node) const {
  ctx.setExprAddr(node.getID(), std::make_shared<IntLiteral>(node.val));
}

void Builder::operator()(const ast::BoolLitExpr &node) const {
  ctx.setExprAddr(node.getID(),
                  std::make_shared<IntLiteral>((Integer)node.val));
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
}

void Builder::operator()(const ast::UnaryExpr &node) const {
  // arithmetic
  if (node.op == ast::UnaryExpr::BitNot || node.op == ast::UnaryExpr::Neg) {
    visit(*node.operand);
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

    auto addr = getElementPtr(node.operand);
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
  visit(*node.operand);
  auto dest = ctx.makeTempLocalReg();
  ctx.setExprAddr(node.getID(), dest);
  auto operand = ctx.getExprAddr(node.operand->getID());
  ctx.emplaceInst<ArithBinaryInst>(dest, ArithBinaryInst::Xor, operand,
                                   std::make_shared<IntLiteral>(1));
}

void Builder::operator()(const ast::BinaryExpr &node) const {
  if (node.op == ast::BinaryExpr::Assign) {
    visit(*node.rhs);
    auto rhsVal = ctx.getExprAddr(node.rhs->getID());
    auto lhsAddr = getElementPtr(node.lhs);
    ctx.emplaceInst<Store>(lhsAddr, rhsVal);
    return;
  }
  if (node.op == ast::BinaryExpr::Subscript ||
      node.op == ast::BinaryExpr::Member) {
    auto addr = getElementPtr(std::static_pointer_cast<ast::Expression>(
        const_cast<ast::BinaryExpr &>(node).shared_from_this()));
    auto val = ctx.makeTempLocalReg("valOrInstPtr");
    ctx.emplaceInst<Load>(val, addr);
    ctx.setExprAddr(node.getID(), val);
    return;
  }
  // Eq, Ne, Lt, Gt, Le, Ge,
  if (ast::BinaryExpr::Eq <= node.op && node.op <= ast::BinaryExpr::Ge) {
    auto op = (RelationInst::OpType)(RelationInst::Eq +
                                     (node.op - ast::BinaryExpr::Eq));
    auto val = ctx.makeTempLocalReg("resV");
    ctx.setExprAddr(node.getID(), val);

    visit(*node.lhs);
    auto lhs = ctx.getExprAddr(node.lhs->getID());
    visit(*node.rhs);
    auto rhs = ctx.getExprAddr(node.rhs->getID());
    ctx.emplaceInst<RelationInst>(val, op, lhs, rhs);
    return;
  }
  if (node.op == ast::BinaryExpr::LogicalAnd ||
      node.op == ast::BinaryExpr::LogicalOr) {
    auto originBB = ctx.getCurBasicBlock();
    auto originLabel = getBBLabel(originBB);
    FunctionModule &func = ctx.getCurFunc();

    auto boolExpAddr = ctx.makeTempLocalReg("boolExpAddr");
    ctx.appendInstFront(func.getFirstBB(),
                        std::make_shared<AllocVar>(boolExpAddr));

    auto rhsFirstBB = func.insertBBAfter(originBB);
    auto rhsFirstLabel = getBBLabel(rhsFirstBB);
    auto successorBB = func.insertBBAfter(rhsFirstBB);
    auto successorLabel = getBBLabel(successorBB);

    visit(*node.lhs);
    auto lhsVal = ctx.getExprAddr(node.lhs->getID());
    ctx.emplaceInst<Store>(boolExpAddr, lhsVal);

    //    auto lhsLastBB = ctx.getCurBasicBlock();
    if (node.op == ast::BinaryExpr::LogicalAnd) {
      ctx.emplaceInst<Branch>(ctx.getExprAddr(node.lhs->getID()), rhsFirstLabel,
                              successorLabel);
    } else {
      ctx.emplaceInst<Branch>(ctx.getExprAddr(node.lhs->getID()),
                              successorLabel, rhsFirstLabel);
    }

    ctx.setCurBasicBlock(rhsFirstBB);
    visit(*node.rhs);
    auto rhsVal = ctx.getExprAddr(node.rhs->getID());
    ctx.emplaceInst<Store>(boolExpAddr, rhsVal);

    //    auto rhsLastBB = ctx.getCurBasicBlock();
    ctx.emplaceInst<Jump>(successorLabel);

    ctx.setCurBasicBlock(successorBB);
    auto val = ctx.makeTempLocalReg("v");
    ctx.setExprAddr(node.getID(), val);

    //    auto boolLit = std::make_shared<IntLiteral>(
    //        (Integer)(node.op == ast::BinaryExpr::LogicalOr));
    //    std::vector<std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>>
    //        phiOptions = {std::make_pair(boolLit, getBBLabel(lhsLastBB)),
    //                      std::make_pair(ctx.getExprAddr(node.rhs->getID()),
    //                                     getBBLabel(rhsLastBB))};
    //    ctx.emplaceInst<Phi>(val, std::move(phiOptions));
    ctx.emplaceInst<Load>(val, boolExpAddr);
    return;
  }
  // string
  if (isStrTy(ctx.getExprType(node.getID()))) {
    visit(*node.lhs);
    visit(*node.rhs);
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
  visit(*node.lhs);
  visit(*node.rhs);
  auto res = ctx.makeTempLocalReg();
  ctx.emplaceInst<ArithBinaryInst>(res, opMap.at(node.op),
                                   ctx.getExprAddr(node.lhs->getID()),
                                   ctx.getExprAddr(node.rhs->getID()));
  ctx.setExprAddr(node.getID(), res);
}

void Builder::operator()(const ast::FuncCallExpr &node) const {
  std::shared_ptr<Addr> dest =
      (bool)ctx.getExprType(node.getID()) ? ctx.makeTempLocalReg() : nullptr;
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
  ctx.appendInstFront(func.getFirstBB(), std::make_shared<AllocVar>(valPtr));

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

void Builder::operator()(const ast::ExprStmt &node) const { visit(*node.expr); }

void Builder::operator()(const ast::ReturnStmt &node) const {
  if (node.expr)
    visit(*node.expr);
  ctx.emplaceInst<Ret>(node.expr ? ctx.getExprAddr(node.expr->getID())
                                 : nullptr);
}

void Builder::operator()(const ast::ContinueStmt &node) const {
  auto originBB = ctx.getCurBasicBlock();
  FunctionModule &func = ctx.getCurFunc();

  auto loopEntry = ctx.getCurLoopEntry();
  ctx.emplaceInst<Jump>(loopEntry);

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
  for (auto &stmt : node.stmts)
    visit(*stmt);
}

void Builder::operator()(const ast::IfStmt &node) const {
  visit(*node.condition);

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

  auto br = std::make_shared<Branch>(
      ctx.getExprAddr(node.condition->getID()), thenFirstLabel,
      node.else_ ? elseFirstLabel : successorLabel);
  ctx.appendInst(originalBB, br);
  auto jump = std::make_shared<Jump>(successorLabel);
  if (!thenLastBB->isCompleted())
    ctx.appendInst(thenLastBB, jump);
  if (node.else_ && !elseLastBB->isCompleted())
    ctx.appendInst(elseLastBB, jump);

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
    ctx.appendInstFront(func.getFirstBB(), std::make_shared<AllocVar>(reg));
    ctx.emplaceInst<Store>(reg, makeReg("0"));
  }
  for (std::size_t i = 0; i < node.formalParameters.size(); ++i) {
    auto name = node.formalParameters[i]->identifier->val;
    auto reg = makeReg(name);
    ctx.appendInstFront(func.getFirstBB(), std::make_shared<AllocVar>(reg));
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
      classLayout.size += 8; // TODO: alignment
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
    auto contentPtrPtr = ctx.makeTempLocalReg("contentPtrPtr");
    ctx.emplaceInst<ArithBinaryInst>(contentPtrPtr, ArithBinaryInst::Add,
                                     arrayInstPtr, makeILit(8));
    auto contentPtr = ctx.makeTempLocalReg("contentPtr");
    ctx.emplaceInst<Load>(contentPtr, contentPtrPtr);

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
  auto defer2 =
      Defer([this, &successorBB] { ctx.setCurBasicBlock(successorBB); });

  auto jump = std::make_shared<Jump>(conditionLabel);
  ctx.appendInst(originBB, jump);

  // condition
  ctx.setCurBasicBlock(conditionFirstBB);
  if (condition)
    visit(*condition);
  else
    ctx.setExprAddr(condition->getID(), makeILit(1));
  auto conditionLastBB = ctx.getCurBasicBlock();
  ctx.pushLoopEntry(conditionLabel);
  ctx.pushLoopSuccessor(successorLabel);
  auto pop = std::shared_ptr<void>(nullptr, [this](void *) {
    ctx.popLoopEntry();
    ctx.popLoopSuccessor();
  });

  auto br = std::make_shared<Branch>(ctx.getExprAddr(condition->getID()),
                                     bodyLabel, successorLabel);
  ctx.appendInst(conditionLastBB, br);

  // body
  ctx.setCurBasicBlock(bodyFirstBB);
  visit(*body);
  // e.g while (...) return;
  if (ctx.getCurBasicBlock()->isCompleted())
    return;
  if (update)
    visit(*update);
  auto bodyLastBB = ctx.getCurBasicBlock();

  // br & jump
  ctx.appendInst(bodyLastBB, jump);
}

std::shared_ptr<Addr> Builder::translateNewArray(
    const std::shared_ptr<ast::Type> &rawCurType,
    std::queue<std::shared_ptr<Addr>> &sizeProvided) const {
  // We describe the translation of statements "new T[N]" here. First, note that
  // the result is a pointer arrayInstPtr, which points to the address of the
  // array instance. It is stored in a temporary register, which is the return
  // value of this function. Hence, the first step is
  //   1. to malloc a piece of memory of length 16 and store the address into
  //      arrayInstPtr.
  // Then, we shall construct the array instance. The first thing to do is
  //   2. to store the length into the first 8 bytes of the instance.
  // After that, we shall deal with the last 8 bytes, which will store the
  // address of the head of the content. We shall process as follow.
  //   3. Calculate the bytes required. Note that this is a java-like language
  //      instead of a C-like one, so the bytes required are expected to be
  //      8 * arraySize if we do not treat bool in a different way.
  //   4. Malloc the memory. The head address will be store in a temporary
  //      register named contentPtr
  //   5. Store the contentPtr into the last 8 bytes of the instance
  // Finally, we initialize each element with a loop. If T is int or bool, then
  // there is nothing to do. If T is a class (or string), then we shall malloc
  // each instance and store its address into the corresponding position. To
  // implement the loop, we first need a temporary variable to store the address
  // of the current element since our registers are of SSA form, that is, they
  // can not be assigned more than once. Namely, we shall
  //  6. alloca a piece of memory for the temporary variable and store the
  //     address into the register curElementPtrPtr. And store contentPtr into
  //     the address stored in curElementPtrPtr. (Meanwhile, we shall compute
  //     the address of the end of the content for later use.)
  // Then, after finishing the routine of creating basic blocks and labels, we
  // construct the condition for the loop to end.
  //   7. Load curElementPtr from the address stored in curElementPtrPtr.
  //   8. Compare it with contentEndPtr and jump based on the result.
  // Now, we implement the body of the loop. In fact, what we need to do is just
  // initialize the current element.
  //   9. New a element. (See next section for details.)
  //   10. Store the address into the current element.
  //   11. Update the loop variable and jump back to the condition basic block.
  //
  // In this passage, we handle some details. First, we need to support newing
  // a jagged array. To achieve this, we let this function to be recursive, that
  // is, in step 9., if the type of the element is still array, then, instead of
  // newing a element, we call this function recursively and pass down the
  // remaining provided sizes.
  // Note that the number of sizes provided may be less than the dimension of
  // the array. This case is similar to the int/bool case. We just return after
  // step 5., leaving the elements uninitialized.

  auto &func = ctx.getCurFunc();
  auto curType = std::dynamic_pointer_cast<ast::ArrayType>(rawCurType);
  assert(curType);
  auto elementType = curType->baseType;

  // 1.
  auto arrayInstPtr = ctx.makeTempLocalReg("arrayInstPtr");
  ctx.emplaceInst<Malloc>(arrayInstPtr, makeILit(16));

  // 2.
  auto size = sizeProvided.front();
  sizeProvided.pop();
  ctx.emplaceInst<Store>(arrayInstPtr, size);

  // 3. ~ 5.
  //  auto elementSize =
  //      std::make_shared<IntLiteral>((Integer)getTypeSize(elementType));
  auto memLen = ctx.makeTempLocalReg("memLen");
  ctx.emplaceInst<AttachedComment>("Calculate the memory needed");
  ctx.emplaceInst<ArithBinaryInst>(memLen, ArithBinaryInst::Mul, makeILit(8),
                                   size); // calcMemLen
  auto contentPtr = ctx.makeTempLocalReg("contentPtr");
  ctx.emplaceInst<Malloc>(contentPtr, memLen); // mallocContent

  auto contentPtrPtr = ctx.makeTempLocalReg("contentPtrPtr");
  ctx.emplaceInst<ArithBinaryInst>(
      contentPtrPtr, ArithBinaryInst::Add, arrayInstPtr,
      std::make_shared<IntLiteral>((Integer)8)); // calcContentPtrPtr
  ctx.emplaceInst<Store>(contentPtrPtr, contentPtr);

  if (isIntTy(elementType) || isBoolTy(elementType))
    return arrayInstPtr;
  if ((bool)std::dynamic_pointer_cast<ast::ArrayType>(elementType) &&
      sizeProvided.empty())
    return arrayInstPtr;

  ctx.emplaceInst<Comment>("init the content of array");
  auto defer = std::shared_ptr<void *>(
      nullptr, [this](void *) { ctx.emplaceInst<Comment>(""); });

  // 6.
  auto curElementPtrPtr = ctx.makeTempLocalReg("curElementPtrPtr");
  ctx.appendInstFront(func.getFirstBB(),
                      std::make_shared<AllocVar>(curElementPtrPtr));
  ctx.emplaceInst<Store>(curElementPtrPtr, contentPtr);
  auto contentEndPtr = ctx.makeTempLocalReg("contentEndPtr");
  ctx.emplaceInst<ArithBinaryInst>(contentEndPtr, ArithBinaryInst::Add,
                                   contentPtr, memLen); // calcContentEndPtr

  auto originBB = ctx.getCurBasicBlock();
  auto conditionFirstBB = func.insertBBAfter(originBB);
  auto conditionLabel = getBBLabel(conditionFirstBB);
  auto bodyFirstBB = func.insertBBAfter(conditionFirstBB);
  auto bodyLabel = getBBLabel(bodyFirstBB);
  auto successorBB = func.insertBBAfter(bodyFirstBB);
  auto successorLabel = getBBLabel(successorBB);

  auto jump2condition = std::make_shared<Jump>(conditionLabel);
  ctx.appendInst(jump2condition);

  // 7.
  ctx.setCurBasicBlock(conditionFirstBB);
  auto curElementPtr = ctx.makeTempLocalReg("curElementPtr");
  ctx.emplaceInst<Load>(curElementPtr, curElementPtrPtr);
  // 8.
  auto cmpRes = ctx.makeTempLocalReg("cmpRes");
  ctx.emplaceInst<RelationInst>(cmpRes, RelationInst::Ne, curElementPtr,
                                contentEndPtr);
  ctx.emplaceInst<Branch>(cmpRes, bodyLabel, successorLabel);

  // 9.
  ctx.setCurBasicBlock(bodyFirstBB);
  auto elementInstPtr =
      (bool)std::dynamic_pointer_cast<ast::ArrayType>(elementType)
          ? translateNewArray(elementType, sizeProvided)
          : makeNewNonarray(elementType);
  // 10.
  ctx.emplaceInst<Store>(curElementPtr, elementInstPtr);
  // 11.
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
  //    BuilderContext::ClassLayout strLayout;
  //    strLayout.size = 16;
  //    ctx.addClassLayout("string", std::move(strLayout));
  //    BuilderContext::ClassLayout arrayLayout;
  //    arrayLayout.size = 16;
  //    ctx.addClassLayout("_array_", std::move(arrayLayout));

  // null
  ctx.addGlobalVar("@null");
  //  ctx.emplaceGlobalInitInst<Malloc>(std::make_shared<GlobalReg>("@null"),
  //                                    makeILit(1));

  auto add = [this](std::string name, std::vector<std::string> args) {
    ctx.addFunc(FunctionModule(std::move(name), std::move(args), true));
  };

  // extern C
  add("memcpy", {"dest", "src", "count"});

  // builtin functions
  add("print", {"str"});
  add("println", {"str"});
  add("getString", {});
  add("getInt", {});
  add("toString", {"i"});

  // builtin functions of string
  add("#string#_ctor_", {"this"});
  add("#string#length", {"this"});
  add("#string#substring", {"this", "left", "right"});
  add("#string#parseInt", {"this"});
  add("#string#ord", {"this", "pos"});
  add("#string#add", {"lhs", "rhs"});

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