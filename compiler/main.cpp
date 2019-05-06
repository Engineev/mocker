#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "ast/ast_node.h"
#include "codegen/instruction_selection.h"
#include "codegen/naive_register_allocation.h"
#include "codegen/peephole.h"
#include "codegen/register_allocation.h"
#include "ir/helper.h"
#include "ir/printer.h"
#include "ir/stats.h"
#include "ir_builder/build.h"
#include "ir_builder/builder.h"
#include "ir_builder/builder_context.h"
#include "nasm/printer.h"
#include "nasm/stats.h"
#include "optim/codegen_prepare.h"
#include "optim/constant_propagation.h"
#include "optim/copy_propagation.h"
#include "optim/dead_code_elimination.h"
#include "optim/function_inline.h"
#include "optim/global_const_inline.h"
#include "optim/global_value_numbering.h"
#include "optim/local_value_numbering.h"
#include "optim/loopinv.h"
#include "optim/memorization.h"
#include "optim/module_simplification.h"
#include "optim/optimizer.h"
#include "optim/promote_global_variables.h"
#include "optim/simplify_cfg.h"
#include "optim/ssa.h"
#include "parse/lexer.h"
#include "parse/parser.h"
#include "semantic/semantic_checker.h"
#include "semantic/sym_tbl.h"

mocker::ir::Module runFrontend(const std::string &srcPath);

void optimize(mocker::ir::Module &module);

mocker::nasm::Module codegen(const mocker::ir::Module &irModule);

void printIRStats(const mocker::ir::Stats &stats);

void printNasmStats(const mocker::nasm::Stats &stats);

void runOptsUntilFixedPoint(mocker::ir::Module &module);

int main(int argc, char **argv) {
  bool semanticOnly = false;
  for (int i = 0; i < argc; ++i) {
    if (std::string(argv[i]) == "--semantic")
      semanticOnly = true;
  }

  auto irModule = runFrontend(argv[1]);

  if (semanticOnly)
    return 0;

  mocker::ir::Stats stats(irModule);
  assert(stats.countInsts<mocker::ir::Phi>() == 0);

  optimize(irModule);

  if (argc >= 3) {
    std::string irPath = argv[2];
    std::ofstream dumpIR(irPath);
    mocker::ir::printModule(irModule, dumpIR);
  }

  auto nasmModule = codegen(irModule);
  if (argc >= 4) {
    std::ofstream fout(argv[3]);
    mocker::nasm::printModule(nasmModule, fout);
    fout.close();
  } else {
    mocker::nasm::printModule(nasmModule, std::cout);
  }

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
  // ATTENTION!
  // * Unreachable blocks should be removed as soon as possible since every
  //   pass that uses the dominator tree requires such blocks having been
  //   removed.

  using namespace mocker;

  mocker::ir::Stats stats(module);
  std::cerr << "Original:\n";
  printIRStats(stats);

  FuncAttr funcAttr;

  runOptPasses<SparseSimpleConstantPropagation>(module);
  runOptPasses<GlobalConstantInline>(module);

  funcAttr.init(module);
  runOptPasses<Memorization>(module, funcAttr, module);
  // ir::printModule(module, std::cerr);

  runOptPasses<FunctionInline>(module);
  runOptPasses<FunctionInline>(module);
  runOptPasses<FunctionInline>(module);
  runOptPasses<UnusedFunctionRemoval>(module);
  runOptPasses<PromoteGlobalVariables>(module);
  ir::verifyModule(module);

  std::cerr << "\nAfter inline and promotion of global variables:\n";
  printIRStats(stats);

  runOptPasses<RewriteBranches>(module);
  runOptPasses<SimplifyPhiFunctions>(module);
  runOptPasses<MergeBlocks>(module);
  runOptPasses<RemoveUnreachableBlocks>(module);

  std::cerr << "\nAfter pre-SSA optimization:\n";
  printIRStats(stats);

  runOptPasses<SSAConstruction>(module);

  runOptsUntilFixedPoint(module);
  funcAttr.init(module);
  runOptPasses<LoopInvariantCodeMotion>(module, funcAttr);
  runOptsUntilFixedPoint(module);

  std::cerr << "\nBefore SSA destruction:\n";
  printIRStats(stats);
  runOptPasses<SSADestruction>(module);
  assert(stats.countInsts<ir::Phi>() == 0);

  std::cerr << "\nAfter SSA destruction:\n";
  printIRStats(stats);

  runOptsUntilFixedPoint(module);

  std::cerr << "\nAfter optimization:\n";
  printIRStats(stats);

  runOptPasses<SSAConstruction>(module);
  for (int i = 0; i < 3; ++i) {
    runOptPasses<SimplifyPhiFunctions>(module);
    runOptPasses<MergeBlocks>(module);
    funcAttr.init(module);
    runOptPasses<DeadCodeElimination>(module, funcAttr);
    runOptPasses<RemoveUnreachableBlocks>(module);
  }

  runOptPasses<CodegenPreparation>(module);

  std::cerr << "\nAfter SSA reconstruction:\n";
  printIRStats(stats);
}

mocker::nasm::Module codegen(const mocker::ir::Module &irModule) {
  using namespace mocker;
  auto res = runInstructionSelection(irModule);
  std::cerr << "\nNASM:\n";
  std::cerr << "After instruction selection:\n";
  printNasmStats(nasm::Stats(res));

  res = allocateRegisters(res);
  std::cerr << "\nAfter register allocation:\n";
  printNasmStats(nasm::Stats(res));

  res = runPeepholeOptimization(res);
  std::cerr << "\nAfter peephole optimization:\n";
  printNasmStats(nasm::Stats(res));
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
  std::size_t cnt = 0;

  bool optimizable = true;
  while (optimizable) {
    ++cnt;
    optimizable = false;

    optimizable |= runOptPasses<RewriteBranches>(module);
    optimizable |= runOptPasses<SimplifyPhiFunctions>(module);
    optimizable |= runOptPasses<MergeBlocks>(module);
    optimizable |= runOptPasses<RemoveUnreachableBlocks>(module);
    optimizable |= runOptPasses<GlobalValueNumbering>(module);
    // optimizable |= runOptPasses<LocalValueNumbering>(module);

    optimizable |= runOptPasses<CopyPropagation>(module);
    optimizable |= runOptPasses<SparseSimpleConstantPropagation>(module);
    optimizable |= runOptPasses<CopyPropagation>(module);
    optimizable |= runOptPasses<SimplifyPhiFunctions>(module);
    optimizable |= runOptPasses<RemoveUnreachableBlocks>(module);
    optimizable |= runOptPasses<RewriteBranches>(module);
    optimizable |= runOptPasses<SimplifyPhiFunctions>(module);
    optimizable |= runOptPasses<MergeBlocks>(module);
    optimizable |= runOptPasses<SimplifyPhiFunctions>(module);
    optimizable |= runOptPasses<RemoveUnreachableBlocks>(module);
    optimizable |= runOptPasses<RemoveTrivialBlocks>(module);
    optimizable |= runOptPasses<SimplifyPhiFunctions>(module);
    optimizable |= runOptPasses<MergeBlocks>(module);
    optimizable |= runOptPasses<RemoveUnreachableBlocks>(module);
    FuncAttr funcAttr;
    funcAttr.init(module);
    optimizable |= runOptPasses<DeadCodeElimination>(module, funcAttr);
    optimizable |= runOptPasses<RemoveUnreachableBlocks>(module);

    ir::verifyModule(module);
  }
}

void printNasmStats(const mocker::nasm::Stats &stats) {
  using namespace mocker::nasm;
  std::cerr << "#Jump = " << stats.countJumps() << std::endl;
  std::cerr << "#insts = " << stats.countInsts() << std::endl;
  std::cerr << "#MemOperation = " << stats.countMemOperation() << std::endl;
}
