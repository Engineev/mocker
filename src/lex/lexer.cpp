#include "lexer.h"

#define ADD(NAME)                                                              \
  add(NAME);                                                                   \
  break;

namespace mocker {

LexError::LexError(StrIter bufferBeg, StrIter bufferEnd, StrIter beg,
                   StrIter end)
    : bufferBeg(bufferBeg), bufferEnd(bufferEnd), beg(beg), end(end) {}

const char *LexError::what() const noexcept {
  if (!msg.empty())
    return msg.c_str();
  using std::to_string;
  Position begPos(bufferBeg, bufferEnd, beg);
  Position endPos(bufferBeg, bufferEnd, end);
  msg = "A lex error occurs between line " + to_string(begPos.line) +
        ", column " + to_string(begPos.col) + " and line " +
        to_string(endPos.line) + ", column " + to_string(endPos.col);
  return msg.c_str();
}


std::vector<Token> Lexer::operator()() {
  tokens.clear();
  curBeg = nxtBeg = srcBeg;

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
    tokens.emplace_back(TokenID::BoolLit, curBeg, nxtBeg, curTok == "true");
  }
  keywordsOrIdentifier(curTok);
  return true;
}

std::string Lexer::getToken() {
  // skip
  while (nxtBeg != srcEnd && !std::ispunct(*nxtBeg) &&
      !std::isalnum(*nxtBeg) && *nxtBeg != '"') {
    if (srcEnd - nxtBeg >= 2 && *nxtBeg == '/' && *(nxtBeg + 1) == '/') {
      while (nxtBeg != srcEnd && *nxtBeg != '\n')
        ++nxtBeg;
    }
  }
  if (nxtBeg == srcEnd)
    return "";

  curBeg = nxtBeg;
  if (std::ispunct(*nxtBeg)) {
    ++nxtBeg;
    return {curBeg, nxtBeg};
  }
  while (nxtBeg != srcEnd && (std::isalnum(*nxtBeg) || *nxtBeg == '_'))
    ++nxtBeg;
  return {curBeg, nxtBeg};
}

std::string Lexer::nextToken() {
  auto tmpCurBeg = curBeg;
  auto tmpNxtBeg = nxtBeg;
  auto res = getToken();
  std::tie(curBeg, nxtBeg) = std::tie(tmpCurBeg, tmpNxtBeg);
  return res;
}

void Lexer::punct(const std::string &curTok) {
  auto add = [this](TokenID id) { tokens.emplace_back(id, curBeg, nxtBeg); };
  switch (curTok[0]) {
  case '(':
    ADD(TokenID::LeftParen);
  case ')':
    ADD(TokenID::RightParen);
  case '[':
    ADD(TokenID::LeftBracket);
  case ']':
    ADD(TokenID::RightBracket);
  case '{':
    ADD(TokenID::LeftBrace);
  case '}':
    ADD(TokenID::RightBrace);
  case ';':
    ADD(TokenID::Semicolon);
  case ',':
    ADD(TokenID::Comma);
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
    ADD(TokenID::Mult);
  case '/':
    ADD(TokenID::Divide);
  case '%':
    ADD(TokenID::Mod);
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
    ADD(TokenID::BitNot);
  case '^':
    ADD(TokenID::BitXor);
  case '.':
    ADD(TokenID::Dot);
  default:
    throw LexError(srcBeg, srcEnd, curBeg, nxtBeg);
  }
}

void Lexer::stringLiteral(const std::string &curTok) {
  assert(curTok[0] == '"');
  auto convert = [this](StrIter beg, StrIter end) -> std::string {
//    const std::string escaped = "abfnrtv\\'\"";
    std::string res;
    for (auto iter = beg + 1; iter < end - 1; ++iter) {
      if (*iter == '\\') {
        if (iter + 1 == end - 1)
          throw LexError(srcBeg, srcEnd, beg, end);
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
          throw LexError(srcBeg, srcEnd, beg, end);
        }
        ++iter;
      } else {
        res.push_back(*iter);
      }
    }
    return res;
  };

  for (auto iter = curBeg; iter != srcEnd; ++iter) {
    if (*iter == '"') {
      tokens.emplace_back(TokenID::StringLit, curBeg, iter + 1,
          convert(curBeg, iter + 1));
      nxtBeg = iter + 1;
      return;
    }
    if (*iter == '\n')
      throw Lexer(curBeg, iter);
  }
  throw Lexer(curBeg, srcEnd);
}

void Lexer::intLiteral(const std::string &curTok) {
  for (auto ch : curTok)
    if (!std::isdigit(ch))
      throw LexError(srcBeg, srcEnd, curBeg, nxtBeg);
  tokens.emplace_back(TokenID::IntLit, curBeg, nxtBeg,
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
    tokens.emplace_back(iter->second, curBeg, nxtBeg);
    return;
  }
  for (auto ch : curTok)
    if (!std::isalnum(ch) && ch != '_')
      throw LexError(srcBeg, srcEnd, curBeg, nxtBeg);
  tokens.emplace_back(iter->second, curBeg, nxtBeg, curTok);
}

} // namespace mocker
