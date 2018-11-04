#ifndef MOCKER_ERROR_H
#define MOCKER_ERROR_H

#include <exception>

#include "defs.h"
#include "position.h"

namespace mocker {

class CompileError : public std::exception {
public:
  CompileError(Position beg, Position end) : beg(beg), end(end) {
    using std::to_string;
    msg = "An error occurs between line " + to_string(beg.line) + ", column " +
          to_string(beg.col) + " and line " + to_string(end.line) +
          ", column " + to_string(end.col);
  }

  const char *what() const noexcept override { return msg.c_str(); }

  const std::pair<Position, Position> position() const {
    return {beg, end};
  }

private:
  std::string msg;
  Position beg, end;
};

} // namespace mocker

#endif // MOCKER_ERROR_H
