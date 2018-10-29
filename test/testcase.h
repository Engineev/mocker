#ifndef MOCKER_TESTCASE_H
#define MOCKER_TESTCASE_H

#include <string>
#include <utility>

namespace mocker {
namespace test {

class TestCase {
public:
  virtual std::pair<bool, std::string> run() = 0;

  virtual ~TestCase() = default;
};

}
}

#endif //MOCKER_TESTCASE_H
