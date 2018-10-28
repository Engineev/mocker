#ifndef MOCKER_POSITION_H
#define MOCKER_POSITION_H

#include <string>

#include "common/defs.h"

namespace mocker {

struct Position {
  Position() = default;
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

  std::size_t line = 0, col = 0;
};

} // namespace mocker

#endif // MOCKER_POSITION_H
