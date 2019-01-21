#ifndef MOCKER_SMALL_MAP_H
#define MOCKER_SMALL_MAP_H

#include <initializer_list>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mocker {

template <class Key, class Val> class SmallMap {
public:
  SmallMap() = default;
  SmallMap(const SmallMap &) = default;
  SmallMap(SmallMap &&) noexcept = default;
  SmallMap &operator=(const SmallMap &) = default;
  SmallMap &operator=(SmallMap &&) noexcept = default;

  SmallMap(const std::initializer_list<std::pair<Key, Val>> &init) {
    for (const auto &kv : init)
      mp.emplace_back(kv);
  }

  bool in(const Key &k) const {
    for (const auto &kv : mp) {
      if (kv.first == k)
        return true;
    }
    return false;
  }

  const Val &at(const Key &k) const {
    for (const auto &kv : mp) {
      if (kv.first == k)
        return kv.second;
    }
    throw std::out_of_range("Key does not exist.");
  }

private:
  std::vector<std::pair<Key, Val>> mp;
};

} // namespace mocker

#endif // MOCKER_SMALL_MAP_H
