#ifndef MOCKER_LEXER_H
#define MOCKER_LEXER_H

#include <bitset>
#include <cctype>
#include <exception>
#include <functional>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>

#include "common/defs.h"
#include "position.h"
#include "token.h"

namespace mocker {

class LexError : public std::exception {
public:
  LexError(StrIter bufferBeg, StrIter bufferEnd, StrIter beg, StrIter end);

  const char *what() const noexcept override;

private:
  StrIter bufferBeg, bufferEnd, beg, end;
  mutable std::string msg;
}; // class LexError

class Lexer {
public:
  Lexer(StrIter beg, StrIter end) : srcBeg(beg), srcEnd(end) {}

  std::vector<Token> operator()();

private:
  StrIter srcBeg, srcEnd; // the begin and end of the entire source code

  // the state of the automata
  StrIter curBeg, nxtBeg; // the begin the processing and next token
  // When getToken() is invoked, curBeg points to the begin of the returned
  // value and nxtBeg points to the begin of the next invoking result, which is
  // also the position next to the last character of this token.
  std::vector<Token> tokens;                        // the tokens generated
  std::function<void(const std::string &)> curNode; // the current DFA node

  // return the next possible token. Comments and whitespaces are skipped. If
  // the input has been exhausted, return an empty string.
  std::string getToken();

  // get the next token without consuming it
  std::string nextToken();

  // DFA nodes
  bool transit(const std::string &curTok);

  void punct(const std::string &curTok);

  void stringLiteral(const std::string &curTok);

  void intLiteral(const std::string & curTok);

  void keywordsOrIdentifier(const std::string & curTok);

}; // namespace Lexer

} // namespace mocker

#endif // MOCKER_LEXER_H
