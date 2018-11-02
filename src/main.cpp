#include <iostream>

#include "lex/lexer.h"
#include "ast/ast_node.h"
#include "ast/printer.h"
#include "parse/parser.h"

int main() {
  using namespace mocker;

  std::shared_ptr<ast::ASTNode> p;
  p = std::make_shared<ast::BuiltinType>(Position(), Position(), ast::BuiltinType::Int);
  p = std::make_shared<ast::ArrayType>(Position(), Position(), std::static_pointer_cast<ast::Type>(p));
  p = std::make_shared<ast::StringLitExpr>(Position(), Position(), std::string("123"));

  std::string src = "a + b - c[1] % d";
  std::cout << "source code = " << src << std::endl;

  std::vector<Token> toks = Lexer(src.begin(), src.end())();
  p = Parser(toks.begin(), toks.end()).expression();

  ast::print(std::cout, *p);

  return 0;
}
