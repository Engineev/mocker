#include <iostream>
#include <memory>
#include "testcase.h"
#include "test_lexer.h"
#include "test_parser.h"

int main() {
  using namespace mocker::test;
  std::vector<std::pair<std::string, std::shared_ptr<TestCase>>> tests = {
      {"TestLexer", std::make_shared<TestLexer>()},
      {"TestParser", std::make_shared<TestParser>()}
  };

  for (auto & test : tests) {
    std::cout << test.first << ": ";
    bool res;
    std::string msg;
    std::tie(res, msg) = test.second->run();
    if (res) {
      std::cout << "Passed";
    } else {
      std::cout << "Failed: " + msg;
    }
    std::cout << std::endl;
  }

  return 0;
}
