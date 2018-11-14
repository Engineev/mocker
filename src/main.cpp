#include <iostream>
#include <fstream>
#include <sstream>

#include "parse/lexer.h"
#include "ast/ast_node.h"
#include "parse/parser.h"
#include "semantic/semantic_checker.h"
#include "ast/sym_tbl.h"

int main(int argc, char **argv) {
  using namespace mocker;

  std::string path = argv[1];
//  std::string path = std::string(CMAKE_SOURCE_DIR) + "/test/test_src.txt";
//  std::string path = std::string(CMAKE_SOURCE_DIR) + "/program.in";

  std::ifstream fin(path);
  std::stringstream sstr;
  sstr << fin.rdbuf();
  std::string src = sstr.str();

  std::vector<Token> toks = Lexer(src.begin(), src.end())();
  Parser parser(toks.begin(), toks.end());
  std::shared_ptr<ast::ASTNode> p = parser.root();
  assert(parser.exhausted());

  auto root = std::static_pointer_cast<ast::ASTRoot>(p);
  assert(root);
  SemanticChecker semantic(root);
  auto syms = semantic.annotate();

  return 0;
}
