#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "ast/ast_node.h"
#include "ir/printer.h"
#include "ir/stats.h"
#include "ir_builder/builder.h"
#include "ir_builder/builder_context.h"
#include "optim/opt_context.h"
#include "optim/optimizer.h"
#include "optim/remove_dead_blocks.h"
#include "optim/ssa.h"
#include "parse/lexer.h"
#include "parse/parser.h"
#include "semantic/semantic_checker.h"
#include "semantic/sym_tbl.h"

void printIRStats(const mocker::ir::Stats &stats) {
  using namespace mocker::ir;
  std::cerr << "#BB = " << stats.countBBs() << std::endl;
  std::cerr << "#insts = " << stats.countInsts<IRInst>() << std::endl;
  std::cerr << "#phis = " << stats.countInsts<Phi>() << std::endl;
  std::cerr << "#store and load "
            << stats.countInsts<Store>() + stats.countInsts<Load>()
            << std::endl;
};

int main(int argc, char **argv) {
  using namespace mocker;

  std::string path = argv[1];
  //  std::string path = std::string(CMAKE_SOURCE_DIR) + "/test/test_src.txt";
  //  std::string path = std::string(CMAKE_SOURCE_DIR) + "/program.in";

  std::ifstream fin(path);
  std::stringstream sstr;
  sstr << fin.rdbuf();
  std::string src = sstr.str();

  std::unordered_map<ast::NodeID, PosPair> pos;

  std::vector<Token> toks = Lexer(src.begin(), src.end())();
  Parser parser(toks.begin(), toks.end(), pos);
  std::shared_ptr<ast::ASTNode> p = parser.root();
  assert(parser.exhausted());

  auto root = std::static_pointer_cast<ast::ASTRoot>(p);
  assert(root);
  SemanticChecker semantic(root, pos);
  semantic.check();
  semantic.renameIdentifiers();

  ir::BuilderContext IRCtx;
  IRCtx.setExprType(semantic.getExprType());
  root->accept(ir::Builder(IRCtx));

  auto &module = IRCtx.getResult();
  for (auto &func : module.getFuncs())
    func.second.buildContext();

  OptContext optCtx(module);
  ir::Stats stats(module);

  std::cerr << "Original:\n";
  printIRStats(stats);

  if (argc == 3) {
    std::string irPath = argv[2];
    std::ofstream dumpIR(irPath + "2.ll");
    ir::Printer(module, dumpIR)();
  }

  runOptPasses<RemoveDeadBlocks>(optCtx);
  runOptPasses<ConstructSSA>(optCtx);

  std::cerr << "\nAfter removing dead blocks & constructing SSA:\n";
  printIRStats(stats);

  if (argc == 3) {
    std::string irPath = argv[2];
    std::ofstream dumpIR(irPath);
    ir::Printer(module, dumpIR)();
  } else {
    ir::Printer{module}();
  }

  return 0;
}
