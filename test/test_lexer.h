#ifndef MOCKER_TEST_LEXER_H
#define MOCKER_TEST_LEXER_H

#include "lex/lexer.h"
#include "lex/token.h"
#include "testcase.h"
#include <utility>

namespace mocker {
namespace test {

class TestLexer : public TestCase {
public:
  ~TestLexer() override = default;

  std::pair<bool, std::string> run() override {
    Position dummy;
    // clang-format off
    std::vector<Entry> tests = {
        {"int", false, Token(TokenID::Int, dummy, dummy)},
        {"int123", false,
              Token(TokenID::Identifier, dummy, dummy, std::string("int123"))},
        {"_a123", true, Token()},
        {"abc+123", false, {
              Token(TokenID::Identifier, dummy, dummy, std::string("abc")),
              Token(TokenID::Plus, dummy, dummy),
              Token(TokenID::IntLit, dummy, dummy, (Integer)123)
          }
        },
        {"abc//1234", false,
         Token(TokenID::Identifier, dummy, dummy, std::string("abc"))},
        {R"("12 3//\"abc")", false,
         Token(TokenID::StringLit, dummy, dummy, std::string(R"(12 3//"abc)"))},
        {"-123", false, {
            Token(TokenID::Minus, dummy, dummy),
            Token(TokenID::IntLit, dummy, dummy, (Integer)123)
          }
        },
        {"++a >> (~b)", false, {
            Token(TokenID::PlusPlus, dummy, dummy),
            Token(TokenID::Identifier, dummy, dummy, std::string("a")),
            Token(TokenID::RightShift, dummy, dummy),
            Token(TokenID::LeftParen, dummy, dummy),
            Token(TokenID::BitNot, dummy, dummy),
            Token(TokenID::Identifier, dummy, dummy, std::string("b")),
            Token(TokenID::RightParen, dummy, dummy)
          }
        },
        {"--\n\n  // comment\n\n  ", false,
         Token(TokenID::MinusMinus, dummy, dummy)}
    };
    // clang-format on
    for (const auto &test : tests) {
      Lexer lex(test.str.begin(), test.str.end());
      std::vector<Token> tokens;
      try {
        tokens = lex();
      } catch (LexError &e) {
        if (test.shouldThrow) {
          continue;
        }
        return {false, test.str};
      }
      if (tokens.size() != test.tokens.size())
        return {false, test.str};
      for (std::size_t i = 0; i < tokens.size(); ++i)
        if (!tokens[i].partialCompare(test.tokens[i]))
          return {false, test.str};
    }

    return {true, ""};
  }

private:
  struct Entry {
    Entry(std::string str, bool shouldThrow, Token token)
        : str(std::move(str)), shouldThrow(shouldThrow),
          tokens({std::move(token)}) {}
    Entry(std::string str, bool shouldThrow, std::vector<Token> tokens)
        : str(std::move(str)), shouldThrow(shouldThrow),
          tokens(std::move(tokens)) {}

    std::string str;
    bool shouldThrow;
    std::vector<Token> tokens;
  };

}; // class TestLexer

} // namespace test
} // namespace mocker

#endif // MOCKER_TEST_LEXER_H
