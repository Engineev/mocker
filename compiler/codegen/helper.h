#ifndef MOCKER_CODEGEN_HELPER_H
#define MOCKER_CODEGEN_HELPER_H

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nasm/module.h"

namespace mocker {

using LineIter = std::list<nasm::Line>::const_iterator;

template <class Set, class Iterable> void unionSet(Set &s, const Iterable &o) {
  for (auto &v : o)
    s.emplace(v);
}

template <class Set, class Iterable> void subSet(Set &s, const Iterable &o) {
  for (auto &v : o) {
    auto iter = s.find(v);
    if (iter != s.end())
      s.erase(iter);
  }
}

template <class Set, class V> bool isIn(const Set &s, const V &v) {
  return s.find(v) != s.end();
}

template <class Set, class Iterable>
void intersectSet(Set &s, const Iterable &o) {
  Set res;
  for (auto &item : o) {
    if (isIn(s, item))
      res.emplace(item);
  }
  s = res;
}

template <class Set, class K> void removeElement(Set &s, const K &val) {
  auto iter = s.find(val);
  assert(iter != s.end());
  s.erase(iter);
}

} // namespace mocker

#endif // MOCKER_CODEGEN_HELPER_H
