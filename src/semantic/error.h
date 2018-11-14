#ifndef MOCKER_SEMANTIC_ERROR_H
#define MOCKER_SEMANTIC_ERROR_H

#include "common/error.h"

namespace mocker {

class UnresolvableSymbol : public CompileError {
public:
  UnresolvableSymbol(const Position &beg, const Position &end,
                     const std::string &symName)
      : CompileError(beg, end) {
    msg = std::string(CompileError::what()) + ". Symbol: " + symName;
  }

  const char *what() const noexcept override { return msg.c_str(); }

private:
  std::string msg;
};

class DuplicatedSymbols : public CompileError {
public:
  DuplicatedSymbols(const Position &beg, const Position &end,
                    const std::string &symName)
      : CompileError(beg, end) {
    msg = std::string(CompileError::what()) + ". Symbol: " + symName;
  }

  const char *what() const noexcept override { return msg.c_str(); }

private:
  std::string msg;
};

class IncompatibleTypes : public CompileError {
public:
  IncompatibleTypes(const Position &beg, const Position &end)
      : CompileError(beg, end) {}
};

class BreakOutOfALoop : public CompileError {
public:
  BreakOutOfALoop(const Position &beg, const Position &end)
      : CompileError(beg, end) {}
};

class ContinueOutOfALoop : public CompileError {
public:
  ContinueOutOfALoop(const Position &beg, const Position &end)
      : CompileError(beg, end) {}
};

class ReturnOutOfAFunction : public CompileError {
public:
  ReturnOutOfAFunction(const Position &beg, const Position &end)
      : CompileError(beg, end) {}
};

class InvalidFunctionCall : public CompileError {
public:
  InvalidFunctionCall(const Position &beg, const Position &end)
      : CompileError(beg, end) {}
};

class InvalidMainFunction : public CompileError {
public:
  InvalidMainFunction(const Position &beg, const Position &end)
      : CompileError(beg, end) {}
};

class InvalidRightValueOperation : public CompileError {
public:
  InvalidRightValueOperation(const Position &beg, const Position &end)
      : CompileError(beg, end) {}
};

} // namespace mocker

#endif // MOCKER_SEMANTIC_ERROR_H
