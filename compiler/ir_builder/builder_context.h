#ifndef MOCKER_BUILDER_CONTEXT_H
#define MOCKER_BUILDER_CONTEXT_H

#include <functional>
#include <list>
#include <stack>
#include <string>
#include <unordered_map>

#include "ast/ast_node.h"
#include "defer.h"
#include "ir/ir_inst.h"
#include "ir/module.h"

namespace mocker {
namespace ir {

class BuilderContext {
  template <class V> using ASTIDMap = std::unordered_map<ast::NodeID, V>;
  template <class V> using InstIDMap = std::unordered_map<InstID, V>;

public:
  explicit BuilderContext(
      const ASTIDMap<std::shared_ptr<mocker::ast::Type>> &exprType);

  Module &getResult();

  // If the expression is a bool literal or int literal, then the return value
  // is just a IntLiteral containing the same value.
  // If the expression is of type bool or int but is not a literal, then the
  // return value is the register containing the value.
  // If the expression is of type array or string or some user-defined type,
  // then the return value is the register containing the pointer which points
  // to the actual instance.
  std::shared_ptr<Addr> getExprAddr(ast::NodeID id) const;

  void setExprAddr(ast::NodeID id, std::shared_ptr<Addr> addr);

  std::shared_ptr<Reg> makeTempLocalReg(const std::string &hint = "");

  template <class InstType, class... Args> void emplaceInst(Args &&... args) {
    auto inst = std::make_shared<InstType>(std::forward<Args>(args)...);
    appendInst(std::move(inst));
  }

  void appendInst(std::shared_ptr<IRInst> inst);

  void appendInst(BBLIter bblIter, std::shared_ptr<IRInst> inst);

  void appendInstFront(BBLIter bblIter, std::shared_ptr<IRInst> inst);

  void setCurBasicBlock(BBLIter val);

  BBLIter getCurBasicBlock() const;

  FunctionModule &getCurFunc() const { return *curFunc; }

  const std::shared_ptr<Label> &getCurLoopEntry() const;

  void pushLoopEntry(std::shared_ptr<Label> entry);

  void popLoopEntry();

  const std::shared_ptr<Label> &getCurLoopSuccessor() const;

  void pushLoopSuccessor(std::shared_ptr<Label> successor);

  void popLoopSuccessor();

  FunctionModule &addFunc(FunctionModule func);

  struct ClassLayout {
    ClassLayout() = default;
    ClassLayout(ClassLayout &&) noexcept = default;
    ClassLayout(const ClassLayout &) = default;

    std::size_t size = 0;
    std::unordered_map<std::string, std::size_t> offset;
  };

  void addClassLayout(std::string className, ClassLayout layout);

  std::size_t getClassSize(const std::string &className) const;

  std::size_t getOffset(const std::string &className,
                        const std::string &varName) const;

  const std::shared_ptr<ast::Type> getExprType(ast::NodeID id) const;

  void initFuncCtx(std::size_t paramNum);

  std::shared_ptr<Reg> addStringLiteral(const std::string &literal);

  void addGlobalVar(std::string ident, std::string data = "12345678");

  template <class Inst, class... Args>
  void emplaceGlobalInitInst(Args &&... args) {
    auto &func = module.getFuncs().at("_init_global_vars");
    func.getMutableBBs().back().appendInst(
        std::make_shared<Inst>(std::forward<Args>(args)...));
  }

  void markExprTrivial(const ast::Expression &node) {
    trivialExpr.emplace(node.getID());
  }

  bool isTrivial(const std::shared_ptr<ast::Expression> &node) const {
    return trivialExpr.find(node->getID()) != trivialExpr.end();
  }

  // Auxiliary information for the translation of logical binary nodes
  struct LogicalExprInfo {
    bool empty = true;
    // the blocks to go when the result is true/false
    BBLIter trueNext, falseNext;

    bool inCondition = false;
  };

  void checkLogicalExpr(const ast::Expression &node) {
    if (logicalExprInfo.empty)
      return;
    emplaceInst<Branch>(
        getExprAddr(node.getID()),
        std::make_shared<Label>(logicalExprInfo.trueNext->getLabelID()),
        std::make_shared<Label>(logicalExprInfo.falseNext->getLabelID()));
  }

  LogicalExprInfo &getLogicalExprInfo() { return logicalExprInfo; }

private:
  LogicalExprInfo logicalExprInfo;

  ASTIDMap<std::shared_ptr<Addr>> exprAddr;
  const ASTIDMap<std::shared_ptr<ast::Type>> &exprType;
  InstIDMap<BBLIter> bbReside;
  std::stack<std::shared_ptr<Label>> loopEntry, loopSuccessor;
  // The key is <class name> + '_' + <variable name>
  std::unordered_map<std::string, ClassLayout> classLayout;
  std::unordered_map<std::string, std::shared_ptr<Reg>> strLits;

  std::unordered_set<ast::NodeID> trivialExpr;

  Module module;

  std::size_t strLitCounter = 0;
  BBLIter curBasicBlock;
  FunctionModule *curFunc = nullptr;
};

} // namespace ir
} // namespace mocker

#endif // MOCKER_BUILDER_CONTEXT_H
