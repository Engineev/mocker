#include "interpreter.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

#include "ir/helper.h"
#include "ir/printer.h"
#include "small_map.h"

//#define PRINT_LOG
#ifdef PRINT_LOG
#include <iostream>
#endif

namespace mocker {
namespace ir {

Interpreter::Interpreter(std::string source) {
  initExternalFuncs();
  parse(deleteComments(std::move(source)));
}

Interpreter::~Interpreter() {
  for (const auto &ptr : malloced)
    std::free(ptr);
}

std::int64_t Interpreter::run() {
  for (auto &var : globalVars) {
    auto &ident = var.getLabel();
    auto &data = var.getData();
    auto res = fastMalloc(data.size());
    std::memcpy(res, data.c_str(), data.size());
    globalReg[ident] = (std::int64_t)(data.c_str());
  }

  return executeFunc("main", {});
}

std::string Interpreter::deleteComments(std::string source) const {
  std::string res;

  source.push_back('\n');
  auto iter = source.cbegin();
  while (iter != source.end()) {
    if (*iter == '\"') {
      auto strEnd = readStringLiteral(iter, source.end());
      res += std::string(iter, strEnd);
      iter = strEnd;
      continue;
    }
    if (*iter != ';') {
      res.push_back(*iter);
      ++iter;
      continue;
    }
    while (*iter != '\n')
      ++iter;
  }

  return res;
}

Interpreter::Iter Interpreter::readStringLiteral(Interpreter::Iter iter,
                                                 Interpreter::Iter end) const {
  assert(iter < end);
  assert(*iter == '\"');
  ++iter;
  while (*iter != '\"') {
    if (*iter == '\\') {
      iter += 2;
      assert(iter < end);
      continue;
    }
    ++iter;
    assert(iter != end);
  }
  return iter + 1;
}

void Interpreter::parse(const std::string &source) {
  std::stringstream ss(source);
  std::string line;
  while (std::getline(ss, line)) {
    if (line.empty())
      continue;
    if (line.at(0) == '@') {
      std::stringstream lineSS(line);
      std::string ident;
      lineSS >> ident;
      ident.pop_back(); // pop the colon
      std::string data;
      char nxt;
      do {
        int ascii;
        lineSS >> ascii;
        data.push_back(char(ascii));
        nxt = lineSS.get();
      } while (nxt == ',');
      globalVars.emplace_back(ident, std::move(data));
      continue;
    }
    std::stringstream lineSS(line);
    std::string buffer;
    lineSS >> buffer;
    assert(buffer == "define");
    std::string funcName;
    lineSS >> funcName;
    FuncModule func;
    lineSS >> buffer;
    assert(buffer == "(");
    while (true) { // read args
      lineSS >> buffer;
      if (buffer == ")")
        break;
      func.args.emplace_back(std::move(buffer));
    }
    lineSS >> buffer;
    if (buffer == "external")
      continue;
    assert(buffer == "{");
    parseFuncBody(ss, func);
    funcs.emplace(std::move(funcName), std::move(func));
  }
}

void Interpreter::parseFuncBody(std::istream &in, FuncModule &func) {
  bool firstBBInitialized = false;
  std::string line;
  while (true) {
    std::getline(in, line);
    if (line == "}")
      break;

    if (line.empty())
      continue;
    if (line[0] == '<') {
      std::size_t label = std::strtoul(&line[0] + 1, nullptr, 10);
      if (!firstBBInitialized) {
        func.firstBB = label;
        firstBBInitialized = true;
      }
      func.label2idx.emplace(label, func.insts.size());
      continue;
    }
    func.insts.emplace_back(parseInst(line));
  }
}

std::shared_ptr<IRInst> Interpreter::parseInst(const std::string &line) const {
  std::stringstream lineSS(line);
  std::string buffer;
  lineSS >> buffer;
  if (buffer == "ret") {
    lineSS >> buffer;
    std::shared_ptr<Addr> val;
    if (buffer == "void")
      val = nullptr;
    else
      val = parseAddr(buffer);
    return std::make_shared<Ret>(val);
  }
  if (buffer == "store") {
    lineSS >> buffer;
    auto dest = parseAddr(buffer);
    lineSS >> buffer;
    auto operand = parseAddr(buffer);
    return std::make_shared<Store>(dest, operand);
  }
  if (buffer == "jump") {
    lineSS >> buffer;
    auto label = std::static_pointer_cast<Label>(parseAddr(buffer));
    return std::make_shared<Jump>(label);
  }
  if (buffer == "br") {
    lineSS >> buffer;
    auto val = parseAddr(buffer);
    lineSS >> buffer;
    auto trueLabel = std::static_pointer_cast<Label>(parseAddr(buffer));
    lineSS >> buffer;
    auto falseLabel = std::static_pointer_cast<Label>(parseAddr(buffer));
    return std::make_shared<Branch>(val, trueLabel, falseLabel);
  }
  auto parseCallRHS =
      [&lineSS, &buffer,
       this](std::shared_ptr<Reg> dest) { // the content after "call"
        std::string funcName;
        lineSS >> funcName;
        std::vector<std::shared_ptr<Addr>> args;
        while (lineSS >> buffer)
          args.emplace_back(parseAddr(buffer));
        return std::make_shared<Call>(std::move(dest), std::move(funcName),
                                      std::move(args));
      };
  if (buffer == "call") {
    return parseCallRHS(nullptr);
  }

  auto dest = std::static_pointer_cast<Reg>(parseAddr(buffer));
  lineSS >> buffer;
  assert(buffer == "=");
  lineSS >> buffer;
  if (buffer == "assign") {
    lineSS >> buffer;
    auto operand = parseAddr(buffer);
    return std::make_shared<Assign>(dest, operand);
  }
  if (buffer == "load") {
    lineSS >> buffer;
    auto addr = parseAddr(buffer);
    return std::make_shared<Load>(dest, addr);
  }
  if (buffer == "call") {
    return parseCallRHS(std::move(dest));
  }
  if (buffer == "alloca") {
    return std::make_shared<Alloca>(dest);
  }
  if (buffer == "malloc") {
    lineSS >> buffer;
    return std::make_shared<Malloc>(dest, parseAddr(buffer));
  }
  if (buffer == "phi") {
    std::vector<std::pair<std::shared_ptr<Addr>, std::shared_ptr<Label>>>
        options;
    while (lineSS >> buffer) {
      assert(buffer == "[");
      lineSS >> buffer;
      auto val = parseAddr(buffer);
      lineSS >> buffer;
      auto label = std::static_pointer_cast<Label>(parseAddr(buffer));
      lineSS >> buffer;
      assert(buffer == "]");
      options.emplace_back(val, label);
    }
    return std::make_shared<Phi>(dest, std::move(options));
  }
  std::string opB = buffer;
  if (opB == "neg" || opB == "bitnot") {
    lineSS >> buffer;
    return std::make_shared<ArithUnaryInst>(
        dest, opB == "neg" ? ArithUnaryInst::Neg : ArithUnaryInst::BitNot,
        parseAddr(buffer));
  }
  using namespace std::string_literals;
  static const SmallMap<std::string, ArithBinaryInst::OpType> BinOp{
      {"bitor"s, ArithBinaryInst::BitOr}, {"bitand"s, ArithBinaryInst::BitAnd},
      {"xor"s, ArithBinaryInst::Xor},     {"shl"s, ArithBinaryInst::Shl},
      {"shr"s, ArithBinaryInst::Shr},     {"add"s, ArithBinaryInst::Add},
      {"sub"s, ArithBinaryInst::Sub},     {"mul"s, ArithBinaryInst::Mul},
      {"div"s, ArithBinaryInst::Div},     {"mod"s, ArithBinaryInst::Mod},
  };
  if (BinOp.in(opB)) {
    lineSS >> buffer;
    auto lhs = parseAddr(buffer);
    lineSS >> buffer;
    auto rhs = parseAddr(buffer);
    return std::make_shared<ArithBinaryInst>(dest, BinOp.at(opB), lhs, rhs);
  }
  static const SmallMap<std::string, RelationInst::OpType> RelOp{
      {"ne"s, RelationInst::OpType::Ne}, {"eq"s, RelationInst::OpType::Eq},
      {"lt"s, RelationInst::OpType::Lt}, {"le"s, RelationInst::OpType::Le},
      {"gt"s, RelationInst::OpType::Gt}, {"ge"s, RelationInst::OpType::Ge}};
  if (RelOp.in(opB)) {
    lineSS >> buffer;
    auto lhs = parseAddr(buffer);
    lineSS >> buffer;
    auto rhs = parseAddr(buffer);
    return std::make_shared<RelationInst>(dest, RelOp.at(opB), lhs, rhs);
  }
  assert(false);
}

std::shared_ptr<Addr> Interpreter::parseAddr(const std::string &str) const {
  assert(!str.empty());
  if (str[0] == '@')
    return std::make_shared<Reg>(str);
  if (str[0] == '%')
    return std::make_shared<Reg>(std::string(str.begin() + 1, str.end()));
  if (str[0] == '<')
    return std::make_shared<Label>(
        (std::size_t)(std::strtoul(&str[0] + 1, nullptr, 10)));
  return std::make_shared<IntLiteral>(
      (Integer)std::strtoll(&str[0], nullptr, 10));
}

void Interpreter::initExternalFuncs() {
  using Args = const std::vector<std::int64_t> &;

  // #_array_#_ctor_ ( this arraySize elementSize )
  externalFuncs.emplace("#_array_#_ctor_",
                        [this](Args args) -> std::int64_t { assert(false); });
  // #_array_#size ( this )
  externalFuncs.emplace("#_array_#size", [](Args args) {
    auto res = *((std::int64_t *)(args[0]) - 1);
    return res;
  });

  // #string#length ( this )
  externalFuncs.emplace("#string#length", [](Args args) {
    auto res = *((std::int64_t *)(args[0]) - 1);
    return res;
  });
  // #string#add ( lhs rhs )
  externalFuncs.emplace("#string#add", [this](Args args) {
    auto lhsLen = externalFuncs["#string#length"]({args[0]});
    auto rhsLen = externalFuncs["#string#length"]({args[1]});
    std::int64_t length = lhsLen + rhsLen;

    char *resInstPtr = (char *)fastMalloc(length + 8);
    *(std::int64_t *)resInstPtr = length;
    resInstPtr += 8;

    std::memcpy(resInstPtr, (void *)args[0], (std::size_t)lhsLen);
    std::memcpy((char *)resInstPtr + lhsLen, (void *)args[1],
                (std::size_t)rhsLen);
    return (std::int64_t)resInstPtr;
  });
  // #string#substring ( this left right )
  externalFuncs.emplace("#string#substring", [this](Args args) {
    auto len = args[2] - args[1];
    char *resInstPtr = (char *)fastMalloc(len + 8);
    *(std::int64_t *)(resInstPtr) = len;
    resInstPtr += 8;

    auto srcContentPtr = (char *)args[0] + args[1];
    std::memcpy(resInstPtr, srcContentPtr, (std::size_t)len);
    return (std::int64_t)resInstPtr;
  });
  // #string#ord ( this pos )
  externalFuncs.emplace("#string#ord", [](Args args) {
    return (std::int64_t) * ((char *)args[0] + args[1]);
  });
  // #string#parseInt ( this )
  externalFuncs.emplace("#string#parseInt", [](Args args) {
    auto len = *((std::int64_t *)args[0] - 1);
    std::string str{(char *)args[0], (char *)args[0] + len};
    return std::stoll(str);
  });
  // #string#_ctor_ ( this )
  externalFuncs.emplace("#string#_ctor_",
                        [](Args args) -> std::int64_t { assert(false); });

  // getInt (  )
  externalFuncs.emplace("getInt", [](Args args) {
    std::int64_t res;
    std::cin >> res;
    return res;
  });
  // print ( str )
  externalFuncs.emplace("print", [this](Args args) {
    auto len = externalFuncs["#string#length"]({args[0]});
    auto str = std::string{(char *)args[0], (char *)args[0] + len};
    std::cout << str;
    return 0;
  });
  // println ( str )
  externalFuncs.emplace("println", [this](Args args) {
    externalFuncs["print"]({args[0]});
    std::cout << std::endl;
    return 0;
  });
  // _printInt ( x )
  externalFuncs.emplace("_printInt", [this](Args args) {
    std::cout << args[0];
    return 0;
  });
  externalFuncs.emplace("_printlnInt", [this](Args args) {
    std::cout << args[0] << std::endl;
    return 0;
  });
  // getString (  )
  externalFuncs.emplace("getString", [this](Args args) {
    std::string str;
    std::cin >> str;

    auto resInstPtr = (char *)fastMalloc(8 + str.length());
    *(std::int64_t *)resInstPtr = (std::int64_t)str.length();
    resInstPtr += 8;
    std::memcpy(resInstPtr, str.c_str(), str.length());
    return (std::int64_t)resInstPtr;
  });
  // toString ( i )
  externalFuncs.emplace("toString", [this](Args args) {
    auto str = std::to_string(args[0]);

    auto resInstPtr = (char *)fastMalloc(8 + str.length());
    *(std::int64_t *)resInstPtr = (std::int64_t)str.length();
    resInstPtr += 8;
    std::memcpy(resInstPtr, str.c_str(), str.length());
    return (std::int64_t)resInstPtr;
  });

  // memcpy ( dest src count )
  externalFuncs.emplace("memcpy", [](Args args) {
    std::memcpy((void *)args[0], (void *)args[1], (std::size_t)args[2]);
    return 0;
  });
}

std::int64_t Interpreter::executeFunc(const FuncModule &func,
                                      const std::vector<std::int64_t> &args) {
  ActivationRecord ar{func};
  assert(func.args.size() == args.size());
  for (std::size_t i = 0; i < args.size(); ++i) {
    ar.localReg[std::to_string(i)] = args[i];
  }

  ars.emplace(std::move(ar));

  std::size_t idx = func.firstBB;
  while (idx != func.insts.size()) {
    idx = executeInst(idx);
  }
  auto res = ars.top().retVal;
  ars.pop();
  return res;
}

void Interpreter::executePhis(const std::vector<std::shared_ptr<Phi>> &phis) {
  struct Change {
    Change(std::shared_ptr<Addr> reg, int64_t val)
        : reg(std::move(reg)), val(val) {}
    std::shared_ptr<Addr> reg;
    std::int64_t val;
  };
  std::vector<Change> changes;
  auto &ar = ars.top();

  for (const auto &p : phis) {
    bool found = false;
    for (auto &option : p->getOptions()) {
      if (option.second->getID() == ar.lastBB) {
        auto val = readVal(option.first);
        changes.emplace_back(p->getDest(), (std::int64_t)val);
        found = true;
        break;
      }
    }
    assert(found);
  }

  for (const auto &change : changes) {
    writeReg(change.reg, change.val);
  }
}

std::int64_t Interpreter::executeFunc(const std::string &funcName,
                                      const std::vector<std::int64_t> &args) {
  // external
  auto iter = externalFuncs.find(funcName);
  if (iter != externalFuncs.end())
    return iter->second(args);
  return executeFunc(funcs.at(funcName), args);
}

std::size_t Interpreter::executeInst(std::size_t idx) {
  auto &ar = ars.top();
  auto inst = ar.curFunc.get().insts.at(idx);
#ifdef PRINT_LOG
  std::cerr << fmtInst(inst) << std::endl;
#endif
  auto &func = ar.curFunc;
  if (auto p = dyc<Assign>(inst)) {
    auto operand = readVal(p->getOperand());
    writeReg(p->getDest(), operand);
    return idx + 1;
  }
  if (auto p = dyc<ArithUnaryInst>(inst)) {
    auto operand = readVal(p->getOperand());
    writeReg(p->getDest(),
             p->getOp() == ArithUnaryInst::OpType::Neg ? -operand : ~operand);
    return idx + 1;
  }
  if (auto p = dyc<ArithBinaryInst>(inst)) {
    auto lhs = readVal(p->getLhs()), rhs = readVal(p->getRhs());
    switch (p->getOp()) {
    case ArithBinaryInst::BitOr:
      writeReg(p->getDest(), lhs | rhs);
      break;
    case ArithBinaryInst::BitAnd:
      writeReg(p->getDest(), lhs & rhs);
      break;
    case ArithBinaryInst::Xor:
      writeReg(p->getDest(), lhs ^ rhs);
      break;
    case ArithBinaryInst::Shl:
      writeReg(p->getDest(), lhs << rhs);
      break;
    case ArithBinaryInst::Shr:
      writeReg(p->getDest(), lhs >> rhs);
      break;
    case ArithBinaryInst::Add:
      writeReg(p->getDest(), lhs + rhs);
      break;
    case ArithBinaryInst::Sub:
      writeReg(p->getDest(), lhs - rhs);
      break;
    case ArithBinaryInst::Mul:
      writeReg(p->getDest(), lhs * rhs);
      break;
    case ArithBinaryInst::Div:
      writeReg(p->getDest(), lhs / rhs);
      break;
    case ArithBinaryInst::Mod:
      writeReg(p->getDest(), lhs % rhs);
      break;
    default:
      assert(false);
    }
    return idx + 1;
  }
  if (auto p = dyc<RelationInst>(inst)) {
    auto lhs = readVal(p->getLhs()), rhs = readVal(p->getRhs());
    std::int64_t res;
    switch (p->getOp()) {
    case RelationInst::Eq:
      res = lhs == rhs;
      break;
    case RelationInst::Ne:
      res = lhs != rhs;
      break;
    case RelationInst::Lt:
      res = lhs < rhs;
      break;
    case RelationInst::Gt:
      res = lhs > rhs;
      break;
    case RelationInst::Le:
      res = lhs <= rhs;
      break;
    case RelationInst::Ge:
      res = lhs >= rhs;
      break;
    default:
      assert(false);
    }
    writeReg(p->getDest(), res);
    return idx + 1;
  }
  if (auto p = dyc<Load>(inst)) {
    auto addr = reinterpret_cast<std::int64_t *>(readVal(p->getAddr()));
    auto val = *addr;
    writeReg(p->getDest(), val);
    return idx + 1;
  }
  if (auto p = dyc<Store>(inst)) {
    std::int64_t val = readVal(p->getVal());
    auto addr = reinterpret_cast<std::int64_t *>(readVal(p->getAddr()));
    *addr = val;
    printLog((std::int64_t)addr, val);
    return idx + 1;
  }
  if (auto p = dyc<Alloca>(inst)) {
    auto res = fastMalloc(8);
    writeReg(p->getDest(), res);
    return idx + 1;
  }
  if (auto p = dyc<Malloc>(inst)) {
    auto sz = (std::size_t)readVal(p->getSize());
    auto res = fastMalloc(sz);
    writeReg(p->getDest(), res);
    return idx + 1;
  }
  if (auto p = dyc<Branch>(inst)) {
    auto condition = readVal(p->getCondition());
    auto nxtLabel = condition ? p->getThen() : p->getElse();
    ar.lastBB = ar.curBB;
    ar.curBB = nxtLabel->getID();
    return func.get().label2idx.at(nxtLabel->getID());
  }
  if (auto p = dyc<Jump>(inst)) {
    ar.lastBB = ar.curBB;
    ar.curBB = p->getLabel()->getID();
    return func.get().label2idx.at(p->getLabel()->getID());
  }
  if (auto p = dyc<Ret>(inst)) {
    if (p->getVal())
      ars.top().retVal = readVal(p->getVal());
    return func.get().insts.size();
  }
  if (auto p = dyc<Call>(inst)) {
    std::vector<std::int64_t> args;
    for (auto &argReg : p->getArgs())
      args.emplace_back(readVal(argReg));
    auto val = executeFunc(p->getFuncName(), args);
    if (p->getDest())
      writeReg(p->getDest(), val);
    return idx + 1;
  }
  if (auto p = dyc<Phi>(inst)) {
    std::vector<std::shared_ptr<Phi>> phis;
    while (true) {
      auto phi = dyc<Phi>(ar.curFunc.get().insts.at(idx));
      if (!phi) {
        executePhis(phis);
        return idx;
      }
      phis.emplace_back(std::move(phi));
      ++idx;
    }
  }
  assert(false);
}

std::int64_t Interpreter::readVal(const std::shared_ptr<Addr> &reg) const {
  if (auto p = dycLocalReg(reg)) {
    if (p->getIdentifier() == ".phi_nan")
      return -123;
    return ars.top().localReg.at(p->getIdentifier());
  }
  if (auto p = dycGlobalReg(reg)) {
    return globalReg.at(p->getIdentifier());
  }
  if (auto p = dyc<IntLiteral>(reg)) {
    return p->getVal();
  }
  assert(false);
}

void *Interpreter::fastMalloc(std::size_t sz) {
  if (curSize + sz <= Capacity && sz <= 128) {
    auto res = (void *)(fastMem + curSize);
    curSize += sz;
    return res;
  }

  auto res = std::malloc(sz);
  malloced.emplace_back(res);
  return res;
}

void Interpreter::printLog(const std::shared_ptr<Addr> &addr,
                           std::int64_t val) {
#ifdef PRINT_LOG
  if (auto p = dyc<Reg>(addr)) {
    std::cerr << p->getIdentifier() << " <- " << val << std::endl;
    return;
  }
#endif
}

void Interpreter::printLog(const std::string &identifier, std::int64_t val) {
#ifdef PRINT_LOG
  std::cerr << identifier << " <- " << val << std::endl;
#endif
}

void Interpreter::printLog(std::int64_t addr, std::int64_t val) {
#ifdef PRINT_LOG
  std::cerr << (std::int64_t)addr << " <- " << val << std::endl;
#endif
}

} // namespace ir
} // namespace mocker
