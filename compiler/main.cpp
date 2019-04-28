#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "ast/ast_node.h"
#include "codegen/naive_instruction_selection.h"
#include "codegen/naive_register_allocation.h"
#include "codegen/register_allocation.h"
#include "ir/helper.h"
#include "ir/printer.h"
#include "ir/stats.h"
#include "ir_builder/build.h"
#include "ir_builder/builder.h"
#include "ir_builder/builder_context.h"
#include "nasm/printer.h"
#include "optim/constant_propagation.h"
#include "optim/copy_propagation.h"
#include "optim/dead_code_elimination.h"
#include "optim/function_inline.h"
#include "optim/module_simplification.h"
#include "optim/optimizer.h"
#include "optim/promote_global_variables.h"
#include "optim/simplify_cfg.h"
#include "optim/ssa.h"
#include "optim/value_numbering.h"
#include "parse/lexer.h"
#include "parse/parser.h"
#include "semantic/semantic_checker.h"
#include "semantic/sym_tbl.h"

mocker::ir::Module runFrontend(const std::string &srcPath);

void optimize(mocker::ir::Module &module);

mocker::nasm::Module codegen(const mocker::ir::Module &irModule);

void printIRStats(const mocker::ir::Stats &stats);

void runOptsUntilFixedPoint(mocker::ir::Module &module);

int main(int argc, char **argv) {
  auto irModule = runFrontend(argv[1]);

  mocker::ir::Stats stats(irModule);
  assert(stats.countInsts<mocker::ir::Phi>() == 0);

  optimize(irModule);

  if (argc == 3) {
    std::string irPath = argv[2];
    std::ofstream dumpIR(irPath);
    mocker::ir::printModule(irModule, dumpIR);
  } else {
    //    mocker::ir::printModule(irModule);
  }

  auto nasmModule = codegen(irModule);
  std::ofstream fout("/Users/aaronren/Desktop/llvm-ir/my.asm");
  mocker::nasm::printModule(nasmModule, fout);
  fout.close();
  mocker::nasm::printModule(nasmModule, std::cout);

  return 0;
}

mocker::ir::Module runFrontend(const std::string &srcPath) {
  std::ifstream fin(srcPath);
  std::stringstream sstr;
  sstr << fin.rdbuf();
  std::string src = sstr.str();

  std::unordered_map<mocker::ast::NodeID, mocker::PosPair> pos;

  std::vector<mocker::Token> toks = mocker::Lexer(src.begin(), src.end())();
  mocker::Parser parser(toks.begin(), toks.end(), pos);
  std::shared_ptr<mocker::ast::ASTNode> p = parser.root();
  assert(parser.exhausted());

  auto root = std::static_pointer_cast<mocker::ast::ASTRoot>(p);
  assert(root);
  mocker::SemanticChecker semantic(root, pos);
  semantic.check();

  auto module = buildIR(root, semantic.getContext());
  for (auto &func : module.getFuncs())
    func.second.buildContext();

  return module;
}

void optimize(mocker::ir::Module &module) {
  using namespace mocker;

  mocker::ir::Stats stats(module);
  std::cerr << "Original:\n";
  printIRStats(stats);

  runOptPasses<FunctionInline>(module);
  runOptPasses<FunctionInline>(module);
  runOptPasses<UnusedFunctionRemoval>(module);
  runOptPasses<PromoteGlobalVariables>(module);
  ir::verifyModule(module);

  std::cerr << "\nAfter inline and promotion of global variables:\n";
  printIRStats(stats);

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
}

mocker::nasm::Module codegen(const mocker::ir::Module &irModule) {
  using namespace mocker;
  auto res = runNaiveInstructionSelection(irModule);
  //  nasm::printModule(res);
  res = allocateRegisters(res);
//  res = allocateRegistersNaively(res);
  return res;
}

void printIRStats(const mocker::ir::Stats &stats) {
  using namespace mocker::ir;
  std::cerr << "#BB = " << stats.countBBs() << std::endl;
  std::cerr << "#insts = " << stats.countInsts<IRInst>() << std::endl;
  std::cerr << "#phis = " << stats.countInsts<Phi>() << std::endl;
  std::cerr << "#store and load "
            << stats.countInsts<Store>() + stats.countInsts<Load>()
            << std::endl;
}

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
