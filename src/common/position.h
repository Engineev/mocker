#ifndef MOCKER_POSITION_H
#define MOCKER_POSITION_H

#include <cassert>
#include <string>
#include <utility>

#include "common/defs.h"

namespace mocker {

struct Position {
  Position() = default;
  Position(std::size_t line, std::size_t col) : line(line), col(col) {}
  Position(StrIter bufferBeg, StrIter bufferEnd, StrIter tok) {
    assert(bufferBeg <= tok && tok < bufferEnd);
    line = 1;
    StrIter bol = bufferBeg; // begin of a line
    for (auto iter = bufferBeg; iter != tok; ++iter) {
      if (*iter == '\n') {
        ++line;
        bol = iter + 1;
      }
    }
    col = tok - bol;
  }
  Position(const Position &) = default;
  Position &operator=(const Position &) = default;

  bool operator==(const Position &rhs) const {
    return line == rhs.line && col == rhs.col;
  }
  bool operator!=(const Position &rhs) const { return !(*this == rhs); }

  std::size_t line = 0, col = 0;
};

using PosPair = std::pair<Position, Position>;

} // namespace mocker

#endif // MOCKER_POSITION_H
