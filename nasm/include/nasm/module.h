// An NASM module contains
//   * directives such as global and extern,
//   * sections such as .text and .data.
// Each section contains one or more lines and a line consists of a label, an
// instruction and some operands. All of these components are optional.

#ifndef MOCKER_NASM_MODULE_H
#define MOCKER_NASM_MODULE_H

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "inst.h"

namespace mocker {
namespace nasm {

struct Line {
  Line(std::string label, std::shared_ptr<Inst> inst)
      : label(std::move(label)), inst(std::move(inst)) {}
  explicit Line(std::string label) : label(std::move(label)) {}
  explicit Line(std::shared_ptr<Inst> inst) : inst(std::move(inst)) {}

  std::string label;
  std::shared_ptr<Inst> inst;
};

class Section {
public:
  using LineIter = std::list<Line>::const_iterator;

  explicit Section(std::string name) : name(std::move(name)) {}

  void labelThisLine(const std::string &label) { lines.emplace_back(label); }

  template <class Type, class... Args>
  void emplaceLine(std::string label, Args &&... args) {
    lines.emplace_back(std::move(label),
                       std::make_shared<Type>(std::forward<Args>(args)...));
  }

  template <class Type, class... Args> void emplaceInst(Args &&... args) {
    emplaceLine<Type>("", std::forward<Args>(args)...);
  }

  void appendInst(std::shared_ptr<nasm::Inst> inst) {
    lines.emplace_back("", std::move(inst));
  }

  void appendLine(Line line) { lines.emplace_back(std::move(line)); }

  void appendLine(LineIter pos, Line line) {
    lines.emplace(pos, std::move(line));
  }

  LineIter erase(LineIter beg, LineIter end) { return lines.erase(beg, end); }

  LineIter erase(LineIter pos) { return lines.erase(pos); }

public:
  const std::string &getName() const { return name; }

  const std::list<Line> &getLines() const { return lines; }

private:
  std::string name;
  std::list<Line> lines;
};

class Module {
public:
  Module() = default;
  Module(const Module &) = default;
  Module(Module &&) = default;
  Module &operator=(const Module &) = default;
  Module &operator=(Module &&) = default;

  template <class Type, class... Args> void emplaceDirective(Args &&... args) {
    directives.emplace_back(
        std::make_shared<Type>(std::forward<Args>(args)...));
  }

  Section &newSection(const std::string &name);

public:
  const std::vector<std::shared_ptr<Directive>> getDirectives() const {
    return directives;
  }

  const std::unordered_map<std::string, Section> &getSections() const {
    return sections;
  }

  Section &getSection(const std::string &name) { return sections.at(name); }

private:
  std::vector<std::shared_ptr<Directive>> directives;
  std::unordered_map<std::string, Section> sections;
};

} // namespace nasm
} // namespace mocker

#endif // MOCKER_NASM_MODULE_H
