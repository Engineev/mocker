#include "builder.h"

#include <functional>

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
  auto addr = ctx.addStringLiteral(node.val);
  ctx.setExprAddr(node.getID(), std::move(addr));
}

void Builder::operator()(const ast::NullLitExpr &node) const {
  ctx.setExprAddr(node.getID(), makeReg("@null"));
}

void Builder::operator()(const ast::IdentifierExpr &node) const {
  auto ty = ctx.getExprType(node.getID());
  if (isIntTy(ty) || isBoolTy(ty)) {
    auto dest = ctx.makeTempLocalReg();
    ctx.setExprAddr(node.getID(), dest);
    ctx.emplaceInst<Load>(dest, makeReg(node.identifier->val));
    return;
  }

  auto addr = makeReg(node.identifier->val);
  ctx.setExprAddr(node.getID(), std::move(addr));
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
    auto val = ctx.makeTempLocalReg();
    ctx.setExprAddr(node.getID(), val);
    ctx.emplaceInst<Load>(addr, val);
    return;
  }
  // Eq, Ne, Lt, Gt, Le, Ge,
  if (ast::BinaryExpr::Eq <= node.op && node.op <= ast::BinaryExpr::Ge) {
    auto op = (RelationInst::OpType)(RelationInst::Eq +
                                     (node.op - ast::BinaryExpr::Eq));
    auto val = ctx.makeTempLocalReg("resV");
    ctx.setExprAddr(node.getID(), val);

    visit(*node.lhs);
    auto lhs = ctx.makeTempLocalReg("lhsV");
    ctx.emplaceInst<Load>(lhs, ctx.getExprAddr(node.lhs->getID()));
    visit(*node.rhs);
    auto rhs = ctx.makeTempLocalReg("rhsV");
    ctx.emplaceInst<Load>(rhs, ctx.getExprAddr(node.rhs->getID()));

    ctx.emplaceInst<RelationInst>(val, op, lhs, rhs);
    return;
  }
  if (node.op == ast::BinaryExpr::LogicalOr ||
      node.op == ast::BinaryExpr::LogicalAnd) {
    auto originBB = ctx.getCurBasicBlock();
    auto originLabel = getBBLabel(originBB);
    FunctionModule &func = originBB->getFuncRef();

    auto rhsBB = func.insertBBAfter(originBB);
    auto rhsLabel = getBBLabel(rhsBB);
    auto successorBB = func.insertBBAfter(rhsBB);
    auto successorLabel = getBBLabel(successorBB);

    visit(*node.lhs);
    // lhsBranch
    ctx.emplaceInst<Branch>(ctx.getExprAddr(node.lhs->getID()), successorLabel,
                            rhsLabel);
    ctx.setCurBasicBlock(rhsBB);
    visit(*node.rhs);
    auto rhsVal = ctx.getExprAddr(node.rhs->getID());
    ctx.emplaceInst<Jump>(successorLabel); // rhsJump

    ctx.setCurBasicBlock(successorBB);
    // phi
    auto val = ctx.makeTempLocalReg("v");
    ctx.setExprAddr(node.getID(), val);
    auto boolLit = std::make_shared<IntLiteral>(
        (Integer)(node.op == ast::BinaryExpr::LogicalOr));
    std::vector<std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>>
        phiOptions = {std::make_pair(boolLit, originLabel),
                      std::make_pair(rhsVal, rhsLabel)};
    ctx.emplaceInst<Phi>(val, std::move(phiOptions));
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
  for (auto &arg : node.args) {
    visit(*arg);
    args.emplace_back(ctx.getExprAddr(arg->getID()));
  }

  ctx.emplaceInst<Call>(dest, funcName, args);

  if (dest)
    ctx.setExprAddr(node.getID(), dest);
}

void Builder::operator()(const ast::NewExpr &node) const {
  auto p = std::dynamic_pointer_cast<ast::ArrayType>(node.type);
  if (!p) {
    auto instancePtr = makeNewNonarray(node.type);
    ctx.setExprAddr(node.getID(), instancePtr);
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
  auto name = node.identifier->val;
  auto valPtr = makeReg(name);
  ctx.emplaceInst<Alloca>(valPtr, 8);

  if (!node.initExpr || isNullTy(ctx.getExprType(node.initExpr->getID())))
    return;

  visit(*node.initExpr);
  ctx.emplaceInst<Store>(makeReg(name),
                         ctx.getExprAddr(node.initExpr->getID()));
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
  FunctionModule &func = originBB->getFuncRef();

  auto loopEntry = ctx.getCurLoopEntry();
  ctx.emplaceInst<Jump>(loopEntry);

  auto successorBB = func.insertBBAfter(originBB);
  ctx.setCurBasicBlock(successorBB);
}

void Builder::operator()(const ast::BreakStmt &node) const {
  auto originBB = ctx.getCurBasicBlock();
  FunctionModule &func = originBB->getFuncRef();

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
  FunctionModule &func = originalBB->getFuncRef();

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
  ctx.appendInst(thenLastBB, jump);
  if (node.else_)
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
  ctx.initFuncCtx();
  ctx.setCurBasicBlock(func.pushBackBB());

  for (auto &p : node.body->stmts)
    visit(*p);
  if (!ctx.getCurBasicBlock()->isCompleted()) {
    ctx.emplaceInst<Ret>();
  }
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
      ctx.addGlobalVar(p->decl->identifier->val, 8);
    }
  }

  for (auto &decl : node.decls) {
    if (!std::dynamic_pointer_cast<ast::VarDecl>(decl))
      visit(*decl);
  }

  std::string initializerName = "_initGlobalVars_";
  auto &func = ctx.addFunc(FunctionModule(initializerName, {}));
  ctx.initFuncCtx();
  ctx.setCurBasicBlock(func.pushBackBB());
  for (auto &decl : node.decls) {
    if (auto p = std::dynamic_pointer_cast<ast::VarDecl>(decl)) {
      if (p->decl->initExpr)
        visit(*p->decl->initExpr);
    }
  }
  ctx.emplaceInst<Ret>();
  auto initGlobal = std::make_shared<Call>(initializerName);
  ctx.appendInst("main", initGlobal);
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
Builder::getElementPtr(const std::shared_ptr<ast::Expression> &exp) const {
  if (auto p = std::dynamic_pointer_cast<ast::IdentifierExpr>(exp)) {
    return makeReg(p->identifier->val);
  }
  if (auto p = std::dynamic_pointer_cast<ast::UnaryExpr>(exp)) {
    assert(p->op == ast::UnaryExpr::PreInc || p->op == ast::UnaryExpr::PreDec);
    return getElementPtr(p->operand);
  }
  auto p = std::dynamic_pointer_cast<ast::BinaryExpr>(exp);
  assert(p);
  visit(*p->lhs);
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
  if (p->op == ast::BinaryExpr::Subscript) {
    visit(*p->rhs);
    auto arrayBasePtr = ctx.makeTempLocalReg();
    // calcBassePtr
    ctx.emplaceInst<ArithBinaryInst>(arrayBasePtr, ArithBinaryInst::OpType::Add,
                                     ctx.getExprAddr(p->lhs->getID()),
                                     std::make_shared<IntLiteral>((Integer)8));
    auto offset = ctx.getExprAddr(p->rhs->getID());
    auto address = ctx.makeTempLocalReg("ptr");
    ctx.emplaceInst<ArithBinaryInst>(address, ArithBinaryInst::OpType::Add,
                                     arrayBasePtr, offset);
    return address;
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
  auto originBB = ctx.getCurBasicBlock();
  FunctionModule &func = originBB->getFuncRef();

  // insert BBs and get labels
  auto conditionFirstBB = func.insertBBAfter(originBB);
  auto conditionLabel = getBBLabel(conditionFirstBB);
  auto bodyFirstBB = func.insertBBAfter(conditionFirstBB);
  auto bodyLabel = getBBLabel(bodyFirstBB);
  auto successorBB = func.insertBBAfter(bodyFirstBB);
  auto successorLabel = getBBLabel(successorBB);

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

  // body
  ctx.setCurBasicBlock(bodyFirstBB);
  visit(*body);
  if (update)
    visit(*update);
  auto bodyLastBB = ctx.getCurBasicBlock();

  // br & jump
  auto br = std::make_shared<Branch>(ctx.getExprAddr(condition->getID()),
                                     bodyLabel, successorLabel);
  ctx.appendInst(conditionLastBB, br);
  auto jump = std::make_shared<Jump>(conditionLabel);
  ctx.appendInst(bodyLastBB, jump);
  ctx.appendInst(originBB, jump);

  ctx.setCurBasicBlock(successorBB);
}

std::shared_ptr<Addr> Builder::translateNewArray(
    const std::shared_ptr<ast::Type> &rawCurType,
    std::queue<std::shared_ptr<Addr>> &sizeProvided) const {
  auto curType = std::dynamic_pointer_cast<ast::ArrayType>(rawCurType);
  if (!curType) {
    return makeNewNonarray(rawCurType);
  }
  auto res = ctx.makeTempLocalReg("p");
  ctx.emplaceInst<Malloc>(res, makeILit(16)); // mallocArrayInstance

  // store the size
  auto size = sizeProvided.front(); // contains the value
  sizeProvided.pop();
  ctx.emplaceInst<Store>(res, size);

  // malloc the content
  auto elementSize =
      std::make_shared<IntLiteral>((Integer)getTypeSize(curType->baseType));
  auto memLen = ctx.makeTempLocalReg("memLen");
  ctx.emplaceInst<ArithBinaryInst>(memLen, ArithBinaryInst::Mul, elementSize,
                                   size); // calcMemLen

  auto contentPtr = ctx.makeTempLocalReg("contentPtr");
  ctx.emplaceInst<Malloc>(contentPtr, memLen); // mallocContent

  auto contentPtrPtr = ctx.makeTempLocalReg("contentPtrPtr");

  ctx.emplaceInst<ArithBinaryInst>(
      contentPtrPtr, ArithBinaryInst::Add, res,
      std::make_shared<IntLiteral>((Integer)8)); // calcContentPtrPtr
  ctx.emplaceInst<Store>(contentPtrPtr, contentPtr);

  // initialize each element via a loop
  // init
  if (sizeProvided.empty())
    return res;
  auto curElementPtrAddr = ctx.makeTempLocalReg("curElementPtrAddr");
  ctx.emplaceInst<Alloca>(curElementPtrAddr, 8);
  ctx.emplaceInst<Store>(curElementPtrAddr, contentPtr); // storeCurElementPtr
  auto contentEndPtr = ctx.makeTempLocalReg();
  ctx.emplaceInst<ArithBinaryInst>(contentEndPtr, ArithBinaryInst::Add,
                                   contentPtr, memLen); // calcContentEndPtr

  auto originBB = ctx.getCurBasicBlock();
  FunctionModule &func = originBB->getFuncRef();
  auto conditionFirstBB = func.insertBBAfter(originBB);
  auto conditionLabel = getBBLabel(conditionFirstBB);
  auto bodyFirstBB = func.insertBBAfter(conditionFirstBB);
  auto bodyLabel = getBBLabel(bodyFirstBB);
  auto successorBB = func.insertBBAfter(bodyFirstBB);
  auto successorLabel = getBBLabel(successorBB);

  auto jump2condition = std::make_shared<Jump>(conditionLabel);
  ctx.appendInst(jump2condition);

  // loop condition
  ctx.setCurBasicBlock(conditionFirstBB);
  auto cmpRes = ctx.makeTempLocalReg("v");
  auto curElementPtr = ctx.makeTempLocalReg();
  ctx.emplaceInst<Load>(curElementPtr, curElementPtrAddr);
  ctx.emplaceInst<RelationInst>(cmpRes, RelationInst::Ne, curElementPtr,
                                contentEndPtr);
  ctx.emplaceInst<Branch>(cmpRes, bodyLabel, successorLabel);

  // body
  ctx.setCurBasicBlock(bodyFirstBB);
  auto elementInst = translateNewArray(curType->baseType, sizeProvided);
  ctx.emplaceInst<Store>(curElementPtr, elementInst); // storeCurElement
  auto nextElementPtr = ctx.makeTempLocalReg();
  ctx.emplaceInst<ArithBinaryInst>(nextElementPtr, ArithBinaryInst::Add,
                                   curElementPtr,
                                   elementSize); // calcNextElementPtr
  ctx.emplaceInst<Store>(curElementPtrAddr, nextElementPtr);
  ctx.appendInst(jump2condition);

  ctx.setCurBasicBlock(successorBB);
  return res;
}

void Builder::addBuiltinAndExternal() const {
  //    BuilderContext::ClassLayout strLayout;
  //    strLayout.size = 16;
  //    ctx.addClassLayout("string", std::move(strLayout));
  //    BuilderContext::ClassLayout arrayLayout;
  //    arrayLayout.size = 16;
  //    ctx.addClassLayout("_array_", std::move(arrayLayout));

  // null
  ctx.addGlobalVar("@null", 8);

  auto add = [this](std::string name, std::vector<std::string> args) {
    ctx.addFunc(FunctionModule(std::move(name), std::move(args), true));
  };

  // extern C
  add("memcpy", {"dest", "src", "count"});
  add("atol", {"str"});

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

std::shared_ptr<LocalReg>
Builder::getMemberElementPtr(const std::shared_ptr<Addr> &base,
                             const std::string &className,
                             const std::string &varName) const {
  auto attachedComment = std::make_shared<AttachedComment>(
      "getElementPtr: " + className + "::" + varName);
  ctx.appendInst(std::move(attachedComment));

  auto elementAddr = ctx.makeTempLocalReg("ptr");
  std::size_t offset = ctx.getOffset(className, varName);
  auto offsetLit = std::make_shared<IntLiteral>((Integer)offset);
  ctx.emplaceInst<ArithBinaryInst>(elementAddr, ArithBinaryInst::Add, base,
                                   offsetLit);
  return elementAddr;
}

std::shared_ptr<Addr> Builder::makeReg(std::string identifier) const {
  if (identifier.at(0) == '@') // is global variable
    return std::make_shared<GlobalReg>(std::move(identifier));
  if (identifier.at(0) == '#') { // is member variable
    std::string className, varName;
    std::tie(className, varName) = splitMemberVarIdent(identifier);
    auto instancePtr = std::make_shared<LocalReg>("this");
    return getMemberElementPtr(instancePtr, className, varName);
  }
  return std::make_shared<LocalReg>(std::move(identifier));
}

std::shared_ptr<Addr>
Builder::makeNewNonarray(const std::shared_ptr<ast::Type> &type) const {
  assert(!std::dynamic_pointer_cast<ast::ArrayType>(type));
  if (auto p = std::dynamic_pointer_cast<ast::BuiltinType>(type))
    assert(p->type == ast::BuiltinType::String);

  auto typeSize = makeILit(getTypeSize(type));
  auto instancePtr = ctx.makeTempLocalReg("p");
  ctx.emplaceInst<Malloc>(instancePtr, typeSize);
  auto ctorName = "#" + getTypeIdentifier(type) + "#_ctor_";
  auto callCtor = std::make_shared<Call>(ctorName, instancePtr);
  return instancePtr;
}

std::shared_ptr<Addr> Builder::makeILit(Integer val) const {
  return std::make_shared<IntLiteral>(val);
}

std::string
Builder::getTypeIdentifier(const std::shared_ptr<ast::Type> &type) const {
  if (auto p = std::dynamic_pointer_cast<ast::BuiltinType>(type)) {
    assert(p->type == ast::BuiltinType::String);
    return "_string_";
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