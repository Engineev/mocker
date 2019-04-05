#include "module.h"

#include <cassert>

namespace mocker {
namespace nasm {

nasm::Section &nasm::Module::newSection(const std::string &name) {
  auto res = sections.emplace(name, Section(name));
  assert(res.second);
  return res.first->second;
}

} // namespace nasm
} // namespace mocker
