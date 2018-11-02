#ifndef MOCKER_TEST_PARSER_H
#define MOCKER_TEST_PARSER_H

#include "lex/lexer.h"
#include "parse/parser.h"
#include "testcase.h"
#include <utility>
#include <vector>

namespace mocker {
namespace test {

class TestParser : public TestCase {
public:
  std::pair<bool, std::string> run() override {
    bool res;
    std::string msg;
    std::tie(res, msg) = parseType();
    if (!res)
      return {false, msg};
    return {true, ""};
  }

private:
  struct Entry {
    Entry(std::string src, bool shouldThrow, bool result)
        : src(std::move(src)), shouldThrow(shouldThrow), result(result) {}

    std::string src;
    bool shouldThrow;
    bool result;
  };

  std::pair<bool, std::string> parseType() {
    // clang-format off
    std::vector<Entry> tests = {
        {"bool", false, true},
        {"abc", false, true},
        {"a[] [  ][]", false, true},
        {"a[1][]", true, false}
    };
    // clang-format on

    for (const auto &test : tests) {
      Lexer lex(test.src.begin(), test.src.end());
      auto toks = lex();
      bool res;
      try {
        Parser parse(toks.begin(), toks.end());
        res = (bool)parse.type();
      } catch (SyntaxError &) {
        if (!test.shouldThrow)
          return {false, test.src + ": should not throw"};
        continue;
      }
      if (res != test.result || test.shouldThrow)
        return {false, test.src};
    }

    return {true, ""};
  }
};

} // namespace test
} // namespace mocker

#endif // MOCKER_TEST_PARSER_H
