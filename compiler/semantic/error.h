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
  UnresolvableSymbol(const PosPair &pos, const std::string &symName)
      : UnresolvableSymbol(pos.first, pos.second, symName) {}

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
  DuplicatedSymbols(const PosPair &pos, const std::string &symName)
      : DuplicatedSymbols(pos.first, pos.second, symName) {}

  const char *what() const noexcept override { return msg.c_str(); }

private:
  std::string msg;
};

class IncompatibleTypes : public CompileError {
public:
  using CompileError::CompileError;
};

class BreakOutOfALoop : public CompileError {
public:
  using CompileError::CompileError;
};

class ContinueOutOfALoop : public CompileError {
public:
  using CompileError::CompileError;
};

class ReturnOutOfAFunction : public CompileError {
public:
  using CompileError::CompileError;
};

class InvalidFunctionCall : public CompileError {
public:
  using CompileError::CompileError;
};

class InvalidMainFunction : public CompileError {
public:
  using CompileError::CompileError;
};

class InvalidRightValueOperation : public CompileError {
public:
  using CompileError::CompileError;
};

} // namespace mocker

#endif // MOCKER_SEMANTIC_ERROR_H
