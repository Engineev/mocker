#include "lexer.h"

#define MOCKER_ADD(NAME)                                                       \
  add(NAME);                                                                   \
  break;

namespace mocker {

Lexer::Lexer(StrIter beg, StrIter end) : srcBeg(beg), srcEnd(end) {}

std::vector<Token> Lexer::operator()() {
  tokens.clear();
  curBeg = nxtBeg = srcBeg;
  curPos.line = 1;
  curPos.col = 1;
  nxtPos = curPos;

  std::string curTok = getToken();
  while (transit(curTok)) {
    curNode(curTok);
    curTok = getToken();
  }
  return std::move(tokens);
}

bool Lexer::transit(const std::string &curTok) {
  namespace ph = std::placeholders;

  if (curTok.empty())
    return false;
  if (curTok[0] == '"') {
    curNode = std::bind(&Lexer::stringLiteral, this, ph::_1);
    return true;
  }
  if (std::ispunct(curTok[0])) {
    assert(curTok.size() == 1);
    curNode = std::bind(&Lexer::punct, this, ph::_1);
    return true;
  }
  if (std::isdigit(curTok[0])) {
    curNode = std::bind(&Lexer::intLiteral, this, ph::_1);
    return true;
  }
  // keywords / identifiers / boolLit
  if (curTok == "true" || curTok == "false") {
    curNode = [this](const std::string & curTok) {
      tokens.emplace_back(TokenID::BoolLit, curPos, nxtPos, curTok == "true");
    };
    return true;
  }
  curNode = std::bind(&Lexer::keywordsOrIdentifier, this, ph::_1);
  return true;
}

std::string Lexer::getToken() {
  // skip the whitespaces and comments
  auto skipWs = [this]() -> bool /* skipped something? */ {
    bool res = false;
    while (nxtBeg != srcEnd && std::isspace(*nxtBeg)) {
      res = true;
      ++nxtPos.col;
      if (*nxtBeg == '\n') {
        ++nxtPos.line;
        nxtPos.col = 1;
      }
      ++nxtBeg;
    }
    return res;
  };
  auto skipComment = [this]() -> bool {
    if (srcEnd - nxtBeg < 2 || *nxtBeg != '/' || *(nxtBeg + 1) != '/')
      return false;
    while (nxtBeg != srcEnd && *nxtBeg != '\n')
      ++nxtBeg;
    if (*nxtBeg == '\n')
      ++nxtBeg;
    ++nxtPos.line;
    nxtPos.col = 1;
    return true;
  };
  while (skipWs() || skipComment())
    ;

  if (nxtBeg == srcEnd)
    return "";

  curBeg = nxtBeg;
  curPos = nxtPos;
  if (std::ispunct(*nxtBeg)) {
    ++nxtBeg;
    ++nxtPos.col;
    return {curBeg, nxtBeg};
  }
  while (nxtBeg != srcEnd && (std::isalnum(*nxtBeg) || *nxtBeg == '_')) {
    ++nxtBeg;
    ++nxtPos.col;
  }
  return {curBeg, nxtBeg};
}

std::string Lexer::nextToken() {
  auto tmpCurBeg = curBeg;
  auto tmpNxtBeg = nxtBeg;
  auto tmpCurPos = curPos;
  auto tmpNxtPos = nxtPos;
  auto res = getToken();
  std::tie(curBeg, nxtBeg, curPos, nxtPos) =
      std::tie(tmpCurBeg, tmpNxtBeg, tmpCurPos, tmpNxtPos);
  return res;
}

void Lexer::punct(const std::string &curTok) {
  auto add = [this](TokenID id) { tokens.emplace_back(id, curPos, nxtPos); };
  switch (curTok[0]) {
  case '(':
    MOCKER_ADD(TokenID::LeftParen);
  case ')':
    MOCKER_ADD(TokenID::RightParen);
  case '[':
    MOCKER_ADD(TokenID::LeftBracket);
  case ']':
    MOCKER_ADD(TokenID::RightBracket);
  case '{':
    MOCKER_ADD(TokenID::LeftBrace);
  case '}':
    MOCKER_ADD(TokenID::RightBrace);
  case ';':
    MOCKER_ADD(TokenID::Semicolon);
  case ',':
    MOCKER_ADD(TokenID::Comma);
  case '+':
    if (nextToken() == "+") {
      getToken();
      add(TokenID::PlusPlus);
    } else
      add(TokenID::Plus);
    break;
  case '-':
    if (nextToken() == "-") {
      getToken();
      add(TokenID::MinusMinus);
    } else
      add(TokenID::Minus);
    break;
  case '*':
    MOCKER_ADD(TokenID::Mult);
  case '/':
    MOCKER_ADD(TokenID::Divide);
  case '%':
    MOCKER_ADD(TokenID::Mod);
  case '>':
    if (nextToken() == "=") {
      getToken();
      add(TokenID::GreaterEqual);
    } else if (nextToken() == ">") {
      getToken();
      add(TokenID::RightShift);
    } else
      add(TokenID::GreaterThan);
    break;
  case '<':
    if (nextToken() == "=") {
      getToken();
      add(TokenID::LessEqual);
    } else if (nextToken() == "<") {
      getToken();
      add(TokenID::LeftShift);
    } else
      add(TokenID::LessThan);
    break;
  case '=':
    if (nextToken() == "=") {
      getToken();
      add(TokenID::Equal);
    } else
      add(TokenID::Assign);
    break;
  case '!':
    if (nextToken() == "=") {
      getToken();
      add(TokenID::NotEqual);
    } else
      add(TokenID::LogicalNot);
    break;
  case '&':
    if (nextToken() == "&") {
      getToken();
      add(TokenID::LogicalAnd);
    } else
      add(TokenID::BitAnd);
    break;
  case '|':
    if (nextToken() == "|") {
      getToken();
      add(TokenID::LogicalOr);
    } else
      add(TokenID::BitOr);
    break;
  case '~':
    MOCKER_ADD(TokenID::BitNot);
  case '^':
    MOCKER_ADD(TokenID::BitXor);
  case '.':
    MOCKER_ADD(TokenID::Dot);
  default:
    throw LexError(curPos, nxtPos);
  }
}

void Lexer::stringLiteral(const std::string &curTok) {
  assert(curTok[0] == '"');
  auto convert = [this](StrIter beg, StrIter end) -> std::string {
    std::string res;
    for (auto iter = beg + 1; iter < end; ++iter) {
      if (*iter == '\\') {
        if (iter + 1 == end)
          throw LexError(curPos, nxtPos);
        char nxt = *(iter + 1);
        switch (nxt) {
        case 'a':
          res.push_back('\a');
          break;
        case 'b':
          res.push_back('\b');
          break;
        case 'f':
          res.push_back('\f');
          break;
        case 'n':
          res.push_back('\n');
          break;
        case 'r':
          res.push_back('\r');
          break;
        case 't':
          res.push_back('\t');
          break;
        case 'v':
          res.push_back('\v');
          break;
        case '\\':
          res.push_back('\\');
          break;
        case '\"':
          res.push_back('"');
          break;
        default:
          throw LexError(curPos, nxtPos);
        }
        ++iter;
      } else {
        res.push_back(*iter);
      }
    }
    return res;
  };

  for (auto iter = curBeg + 1; iter != srcEnd; ++iter) {
    if (*iter == '"') {
      nxtBeg = iter + 1;
      tokens.emplace_back(TokenID::StringLit, curPos, nxtPos,
                          convert(curBeg, iter));
      return;
    }
    if (*iter == '\\') {
      ++iter;
      if (iter == srcEnd)
        throw LexError(curPos, nxtPos);
    }
    if (*iter == '\n')
      throw LexError(curPos, nxtPos);
  }
  throw LexError(curPos, nxtPos);
}

void Lexer::intLiteral(const std::string &curTok) {
  for (auto ch : curTok)
    if (!std::isdigit(ch))
      throw LexError(curPos, nxtPos);
  tokens.emplace_back(TokenID::IntLit, curPos, nxtPos,
      (Integer)std::atoll(curTok.c_str()));
}

void Lexer::keywordsOrIdentifier(const std::string &curTok) {
  static const std::unordered_map<std::string, TokenID> kw = {
      {"bool", TokenID::Bool},
      {"int", TokenID::Int},
      {"string", TokenID::String},
      {"void", TokenID::Void},
      {"null", TokenID::Null},
      {"if", TokenID::If},
      {"else", TokenID::Else},
      {"for", TokenID::For},
      {"while", TokenID::While},
      {"break", TokenID::Break},
      {"continue", TokenID::Continue},
      {"return", TokenID::Return},
      {"new", TokenID::New},
      {"class", TokenID::Class},
      {"this", TokenID::This}
  };
  auto iter = kw.find(curTok);
  if (iter != kw.end()) {
    tokens.emplace_back(iter->second, curPos, nxtPos);
    return;
  }
  for (auto ch : curTok)
    if (!std::isalnum(ch) && ch != '_')
      throw LexError(curPos, nxtPos);
  tokens.emplace_back(TokenID::Identifier, curPos, nxtPos, curTok);
}


} // namespace mocker
