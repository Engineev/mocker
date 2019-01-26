#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "ast/ast_node.h"
#include "ir/printer.h"
#include "ir_builder/builder.h"
#include "ir_builder/builder_context.h"
#include "parse/lexer.h"
#include "parse/parser.h"
#include "semantic/semantic_checker.h"
#include "semantic/sym_tbl.h"

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

  if (argc == 3) {
    std::string irPath = argv[2];
    std::ofstream dumpIR(irPath);
    ir::Printer(IRCtx.getResult(), dumpIR)();
  } else {
    ir::Printer(IRCtx.getResult())();
  }

  return 0;
}
