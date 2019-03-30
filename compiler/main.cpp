#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "ast/ast_node.h"
#include "ir/helper.h"
#include "ir/printer.h"
#include "ir/stats.h"
#include "ir_builder/build.h"
#include "ir_builder/builder.h"
#include "ir_builder/builder_context.h"
#include "optim/constant_propagation.h"
#include "optim/copy_propagation.h"
#include "optim/dead_code_elimination.h"
#include "optim/function_inline.h"
#include "optim/optimizer.h"
#include "optim/promote_global_variables.h"
#include "optim/simplify_cfg.h"
#include "optim/ssa.h"
#include "optim/value_numbering.h"
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

void runOptsUntilFixedPoint(mocker::ir::Module &module) {
  using namespace mocker;

  bool optimizable = true;
  while (optimizable) {
    optimizable = false;
    // std::cerr << "\n=======\n";
    // std::cerr << optimizable;
    optimizable |= runOptPasses<LocalValueNumbering>(module);
    // std::cerr << optimizable;
    optimizable |= runOptPasses<CopyPropagation>(module);
    // std::cerr << optimizable;
    optimizable |= runOptPasses<SparseSimpleConstantPropagation>(module);
    // std::cerr << optimizable;
    optimizable |= runOptPasses<CopyPropagation>(module);
    // std::cerr << optimizable;
    optimizable |= runOptPasses<SimplifyPhiFunctions>(module);
    optimizable |= runOptPasses<RewriteBranches>(module);
    // std::cerr << optimizable;
    optimizable |= runOptPasses<SimplifyPhiFunctions>(module);
    optimizable |= runOptPasses<MergeBlocks>(module);
    // std::cerr << optimizable;
    optimizable |= runOptPasses<SimplifyPhiFunctions>(module);
    optimizable |= runOptPasses<RemoveUnreachableBlocks>(module);
    // std::cerr << optimizable;
    optimizable |= runOptPasses<RemoveTrivialBlocks>(module);
    // std::cerr << optimizable;
    optimizable |= runOptPasses<DeadCodeElimination>(module);
    // std::cerr << optimizable;
  }
}

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

  auto module = buildIR(root, semantic.getContext());
  for (auto &func : module.getFuncs())
    func.second.buildContext();

  ir::Stats stats(module);

  std::cerr << "Original:\n";
  printIRStats(stats);
  assert(stats.countInsts<ir::Phi>() == 0);

  runOptPasses<FunctionInline>(module);
  runOptPasses<FunctionInline>(module);
//  runOptPasses<PromoteGlobalVariables>(module);
  ir::verifyModule(module);

  std::cerr << "\nAfter inline and promotion of global variables:\n";
  printIRStats(stats);

  if (argc == 3) {
    std::string irPath = argv[2];
    std::ofstream dumpIR(irPath);
    ir::printModule(module, dumpIR);
  }

  runOptsUntilFixedPoint(module);

  std::cerr << "\nAfter pre-SSA optimization:\n";
  printIRStats(stats);

  runOptPasses<SSAConstruction>(module);

  runOptsUntilFixedPoint(module);

  std::cerr << "\nBefore SSA destruction:\n";
  printIRStats(stats);

  runOptPasses<SSADestruction>(module);
  assert(stats.countInsts<ir::Phi>() == 0);

  runOptsUntilFixedPoint(module);

  std::cerr << "\nAfter optimization:\n";
  printIRStats(stats);

  if (argc == 3) {
    std::string irPath = argv[2];
    std::ofstream dumpIR(irPath);
    ir::printModule(module, dumpIR);
  } else {
    ir::printModule(module);
  }

  return 0;
}
