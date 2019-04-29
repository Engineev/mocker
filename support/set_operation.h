#ifndef MOCKER_SET_OPERATION_H
#define MOCKER_SET_OPERATION_H

namespace mocker {

template <class Set, class T> void unionSet(Set &s, const T &xs) {
  for (auto &x : xs) {
    s.emplace(x);
  }
}

template <class Set, class T> Set intersectSets(const Set &s, const T &xs) {
  Set res;
  for (auto &x : xs) {
    if (s.find(x) != s.end())
      res.emplace(x);
  }
  return res;
}

template <class Set, class T> void intersectSet(Set &s, const T &xs) {
  s = intersectSets(s, xs);
}

template <class Set, class T> bool isIn(const Set &s, const T &x) {
  return s.find(x) != s.end();
}

} // namespace mocker

#endif // MOCKER_SET_OPERATION_H
