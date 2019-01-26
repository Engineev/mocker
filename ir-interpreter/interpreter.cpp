#include "interpreter.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

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
  for (auto &func : globalVarInit)
    executeFunc(func, {});
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
      FuncModule func;
      parseFuncBody(ss, func);
      globalVarInit.emplace_back(std::move(func));
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
  if (buffer == "strcpy") {
    lineSS >> buffer;
    auto dest = parseAddr(buffer);
    while (lineSS.peek() != '\"')
      lineSS.get();
    lineSS.get();
    std::getline(lineSS, buffer);
    assert(buffer.back() == '\"');
    buffer.pop_back();
    std::string data;
    for (auto iter = buffer.begin(); iter != buffer.end(); ++iter) {
      if (*iter == '\\') {
        ++iter;
        if (*iter == 'n')
          data.push_back('\n');
        else if (*iter == '\\')
          data.push_back('\\');
        else if (*iter == '\"')
          data.push_back('\"');
        else
          assert(false);
        continue;
      }
      data.push_back(*iter);
    }
    return std::make_shared<StrCpy>(dest, std::move(data));
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
       this](std::shared_ptr<Addr> dest) { // the content after "call"
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

  auto dest = parseAddr(buffer);
  lineSS >> buffer;
  assert(buffer == "=");
  lineSS >> buffer;
  if (buffer == "load") {
    lineSS >> buffer;
    auto addr = parseAddr(buffer);
    return std::make_shared<Load>(dest, addr);
  }
  if (buffer == "call") {
    return parseCallRHS(std::move(dest));
  }
  if (buffer == "alloca") {
    std::size_t size;
    lineSS >> size;
    return std::make_shared<Alloca>(dest, size);
  }
  if (buffer == "salloc") {
    std::size_t size;
    lineSS >> size;
    return std::make_shared<SAlloc>(dest, size);
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
    return std::make_shared<GlobalReg>(str);
  if (str[0] == '%')
    return std::make_shared<LocalReg>(std::string(str.begin() + 1, str.end()));
  if (str[0] == '<')
    return std::make_shared<Label>(
        (std::size_t)(std::strtoul(&str[0] + 1, nullptr, 10)));
  return std::make_shared<IntLiteral>(
      (Integer)std::strtoll(&str[0], nullptr, 10));
}

void Interpreter::initExternalFuncs() {
  using Args = const std::vector<std::int64_t> &;

  // #_array_#_ctor_ ( this arraySize elementSize )
  externalFuncs.emplace("#_array_#_ctor_", [this](Args args) {
    //    assert(false);
    auto instPtr = reinterpret_cast<char *>(args[0]);
    *(std::int64_t *)(instPtr) = args[1];
    malloced.emplace_back(std::malloc((std::size_t)(args[1] * args[2])));
    *(void **)(instPtr + 8) = malloced.back();
    return 0;
  });
  // #_array_#size ( this )
  externalFuncs.emplace("#_array_#size", [](Args args) {
    auto res = *(std::int64_t *)args[0];
    return *(std::int64_t *)args[0];
  });

  // #string#length ( this )
  externalFuncs.emplace("#string#length",
                        [](Args args) { return *(std::int64_t *)(args[0]); });
  // #string#add ( lhs rhs )
  externalFuncs.emplace("#string#add", [this](Args args) {
    //    assert(false);
    //    return 0;
    auto resInstPtr = std::malloc(16);
    malloced.emplace_back(resInstPtr);

    auto lhsLen = externalFuncs["#string#length"]({args[0]});
    auto rhsLen = externalFuncs["#string#length"]({args[1]});
    std::int64_t length = lhsLen + rhsLen;
    *(std::int64_t *)resInstPtr = length;

    auto resContentPtr = std::malloc((std::size_t)length);
    malloced.emplace_back(resContentPtr);
    *((void **)(resInstPtr) + 1) = (void *)resContentPtr;

    auto lhsContentPtr = *(void **)((char *)args[0] + 8);
    auto rhsContentPtr = *(void **)((char *)args[1] + 8);
    std::memcpy(resContentPtr, lhsContentPtr, (std::size_t)lhsLen);
    std::memcpy((char *)resContentPtr + lhsLen, rhsContentPtr,
                (std::size_t)rhsLen);
    return (std::int64_t)resInstPtr;
  });
  // #string#substring ( this left right )
  externalFuncs.emplace("#string#substring", [this](Args args) {
    //    assert(false);
    //    return 0;
    auto resInstPtr = std::malloc(16);
    malloced.emplace_back(resInstPtr);

    auto len = args[2] - args[1];
    *(std::int64_t *)(resInstPtr) = len;

    auto resContentPtr = std::malloc((std::size_t)len);
    malloced.emplace_back(resContentPtr);
    auto resContentPtrPtr = (char *)resInstPtr + 8;
    *(char **)resContentPtrPtr = (char *)resContentPtr;

    auto srcContentPtrPtr = (char *)args[0] + 8;
    auto srcContentPtr = *(char **)(srcContentPtrPtr) + args[1];
    std::memcpy(resContentPtr, srcContentPtr, (std::size_t)len);
    return (std::int64_t)resInstPtr;
  });
  // #string#ord ( this pos )
  externalFuncs.emplace("#string#ord", [](Args args) {
    return (std::int64_t) * (*(char **)((char *)args[0] + 8) + args[1]);
    //    auto contentPtrPtr = (char *)args[0] + 8;
    //    auto contentPtr = *(char**)(contentPtrPtr);
    //    return (std::int64_t)*(contentPtr + args[1]);
  });
  // #string#parseInt ( this )
  externalFuncs.emplace("#string#parseInt", [](Args args) {
    auto len = *(std::int64_t *)(args[0]);
    auto contentPtrPtr = (char *)args[0] + 8;
    auto contentPtr = *(char **)contentPtrPtr;
    std::string str{contentPtr, contentPtr + len};
    return std::stoll(str);
  });
  // #string#_ctor_ ( this )
  externalFuncs.emplace("#string#_ctor_", [](Args args) {
    *(std::int64_t *)args[0] = 0;
    return 0;
  });

  // getInt (  )
  externalFuncs.emplace("getInt", [](Args args) {
    std::int64_t res;
    std::cin >> res;
    return res;
  });
  // print ( str )
  externalFuncs.emplace("print", [this](Args args) {
    auto len = externalFuncs["#string#length"]({args[0]});
    auto contentPtrPtr = (char **)args[0] + 1;
    auto contentPtr = *contentPtrPtr;
    std::cout << std::string{contentPtr, contentPtr + len};
    return 0;
  });
  // println ( str )
  externalFuncs.emplace("println", [this](Args args) {
    externalFuncs["print"]({args[0]});
    std::cout << std::endl;
    return 0;
  });
  // getString (  )
  externalFuncs.emplace("getString", [this](Args args) {
    //    assert(false);
    //    return 0;
    auto resInstPtr = std::malloc(16);
    malloced.emplace_back(resInstPtr);

    std::string str;
    std::cin >> str;

    *(std::int64_t *)resInstPtr = (std::int64_t)str.length();

    auto resContentPtr = std::malloc(str.length());
    malloced.emplace_back(resContentPtr);
    *((char **)(resInstPtr) + 1) = (char *)resContentPtr;
    std::memcpy(resContentPtr, str.c_str(), str.length());

    return (std::int64_t)resInstPtr;
  });
  // toString ( i )
  externalFuncs.emplace("toString", [this](Args args) {
    //    assert(false);
    //    return 0;
    auto str = std::to_string(args[0]);

    auto resInstPtr = std::malloc(16);
    malloced.emplace_back(resInstPtr);

    *(std::int64_t *)resInstPtr = (std::int64_t)str.length();

    auto resContentPtr = std::malloc(str.length());
    malloced.emplace_back(resContentPtr);
    *((char **)(resInstPtr) + 1) = (char *)resContentPtr;
    std::memcpy(resContentPtr, str.c_str(), str.length());

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
    //    printLog(func.args[i], args[i]);
  }

  ars.emplace(std::move(ar));
  std::size_t idx = func.firstBB;
  while (idx != func.insts.size())
    idx = executeInst(idx);
  auto res = ars.top().retVal;
  ars.pop();
  return res;
}

std::int64_t Interpreter::executeFunc(const std::string &funcName,
                                      const std::vector<std::int64_t> &args) {
  // external
  auto iter = externalFuncs.find(funcName);
  if (iter != externalFuncs.end())
    return iter->second(args);
  return executeFunc(funcs.at(funcName), args);
}

std::size_t Interpreter::executeInst(const std::size_t idx) {
  auto &ar = ars.top();
  auto inst = ar.curFunc.get().insts.at(idx);
#ifdef PRINT_LOG
  std::cerr << fmtInst(inst) << std::endl;
#endif
  auto &func = ar.curFunc;
  if (auto p = std::dynamic_pointer_cast<ArithUnaryInst>(inst)) {
    auto operand = readVal(p->operand);
    writeReg(p->dest,
             p->op == ArithUnaryInst::OpType::Neg ? -operand : ~operand);
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<ArithBinaryInst>(inst)) {
    auto lhs = readVal(p->lhs), rhs = readVal(p->rhs);
    switch (p->op) {
    case ArithBinaryInst::BitOr:
      writeReg(p->dest, lhs | rhs);
      break;
    case ArithBinaryInst::BitAnd:
      writeReg(p->dest, lhs & rhs);
      break;
    case ArithBinaryInst::Xor:
      writeReg(p->dest, lhs ^ rhs);
      break;
    case ArithBinaryInst::Shl:
      writeReg(p->dest, lhs << rhs);
      break;
    case ArithBinaryInst::Shr:
      writeReg(p->dest, lhs >> rhs);
      break;
    case ArithBinaryInst::Add:
      writeReg(p->dest, lhs + rhs);
      break;
    case ArithBinaryInst::Sub:
      writeReg(p->dest, lhs - rhs);
      break;
    case ArithBinaryInst::Mul:
      writeReg(p->dest, lhs * rhs);
      break;
    case ArithBinaryInst::Div:
      writeReg(p->dest, lhs / rhs);
      break;
    case ArithBinaryInst::Mod:
      writeReg(p->dest, lhs % rhs);
      break;
    default:
      assert(false);
    }
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<RelationInst>(inst)) {
    auto lhs = readVal(p->lhs), rhs = readVal(p->rhs);
    std::int64_t res;
    switch (p->op) {
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
    writeReg(p->dest, res);
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<Load>(inst)) {
    auto addr = reinterpret_cast<std::int64_t *>(readVal(p->addr));
    auto val = *addr;
    //    auto addr = reinterpret_cast<void*>(readVal(p->addr));
    //    auto val = memSim.read(addr);
    writeReg(p->dest, val);
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<Store>(inst)) {
    std::int64_t val = readVal(p->operand);
    auto addr = reinterpret_cast<std::int64_t *>(readVal(p->dest));
    *addr = val;
    //    auto addr = (void*)readVal(p->dest);
    //    memSim.write(addr, val, inst);
    printLog((std::int64_t)addr, val);
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<StrCpy>(inst)) {
    auto dest = (void *)readVal(p->dest);
    std::memcpy(dest, p->data.c_str(), p->data.size());
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<Alloca>(inst)) {
    malloced.emplace_back(std::malloc(p->size));
    auto res = malloced.back();
    //    auto res = memSim.allocA(p->size, inst);
    writeReg(p->dest, res);
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<SAlloc>(inst)) {
    malloced.emplace_back(std::malloc(p->size));
    //    auto res = memSim.salloc(p->size, inst);
    writeReg(p->dest, malloced.back());
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<Malloc>(inst)) {
    auto sz = readVal(p->size);
    malloced.emplace_back(std::malloc((std::size_t)sz));
    //    auto res = memSim.malloc(sz, inst);
    writeReg(p->dest, malloced.back());
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<Branch>(inst)) {
    auto condition = readVal(p->condition);
    auto nxtLabel = condition ? p->then : p->else_;
    ar.lastBB = ar.curBB;
    ar.curBB = nxtLabel->id;
    return func.get().label2idx.at(nxtLabel->id);
  }
  if (auto p = std::dynamic_pointer_cast<Jump>(inst)) {
    ar.lastBB = ar.curBB;
    ar.curBB = p->dest->id;
    return func.get().label2idx.at(p->dest->id);
  }
  if (auto p = std::dynamic_pointer_cast<Ret>(inst)) {
    if (p->val)
      ars.top().retVal = readVal(p->val);
    return func.get().insts.size();
  }
  if (auto p = std::dynamic_pointer_cast<Call>(inst)) {
    std::vector<std::int64_t> args;
    for (auto &argReg : p->args)
      args.emplace_back(readVal(argReg));
    auto val = executeFunc(p->funcName, args);
    if (p->dest)
      writeReg(p->dest, val);
    return idx + 1;
  }
  if (auto p = std::dynamic_pointer_cast<Phi>(inst)) {
    for (auto &option : p->options) {
      if (option.second->id == ar.lastBB) {
        auto val = readVal(option.first);
        writeReg(p->dest, val);
        return idx + 1;
      }
    }
    assert(false);
  }
  assert(false);
}

std::int64_t Interpreter::readVal(const std::shared_ptr<Addr> &reg) const {
  if (auto p = std::dynamic_pointer_cast<LocalReg>(reg)) {
    return ars.top().localReg.at(p->identifier);
  }
  if (auto p = std::dynamic_pointer_cast<GlobalReg>(reg)) {
    return globalReg.at(p->identifier);
  }
  if (auto p = std::dynamic_pointer_cast<IntLiteral>(reg)) {
    return p->val;
  }
  assert(false);
}

void Interpreter::printLog(const std::shared_ptr<Addr> &addr,
                           std::int64_t val) {
#ifdef PRINT_LOG
  if (auto p = std::dynamic_pointer_cast<LocalReg>(addr)) {
    std::cerr << p->identifier << " <- " << val << std::endl;
    return;
  }
  if (auto p = std::dynamic_pointer_cast<GlobalReg>(addr)) {
    std::cerr << p->identifier << " <- " << val << std::endl;
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
