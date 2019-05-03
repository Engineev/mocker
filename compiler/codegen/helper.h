#ifndef MOCKER_CODEGEN_HELPER_H
#define MOCKER_CODEGEN_HELPER_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nasm/module.h"
#include "set_operation.h"

namespace mocker {

using LineIter = std::list<nasm::Line>::const_iterator;

template <class Set, class K> void removeElement(Set &s, const K &val) {
  auto iter = s.find(val);
  assert(iter != s.end());
  s.erase(iter);
}

} // namespace mocker

#endif // MOCKER_CODEGEN_HELPER_H
