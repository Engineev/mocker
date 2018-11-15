#ifndef MOCKER_SCOPE_ID_H
#define MOCKER_SCOPE_ID_H

#include <cstddef>
#include <functional>
#include <vector>

namespace mocker {
namespace ast {

class ScopeID {
public:
  ScopeID() = default;
  ScopeID(const ScopeID &) = default;
  ScopeID(ScopeID &&) noexcept = default;
  ScopeID &operator=(const ScopeID &) = default;
  ScopeID &operator=(ScopeID &&) noexcept = default;
  ~ScopeID() = default;

  bool operator==(const ScopeID &rhs) const {
    if (rhs.ids.size() != ids.size())
      return false;
    for (std::size_t i = 0; i < ids.size(); ++i)
      if (ids[i] != rhs.ids[i])
        return false;
    return true;
  }

private:
  friend class SymTbl;
  ScopeID(std::vector<std::size_t> ids) : ids(std::move(ids)) {}

  std::vector<std::size_t> ids;
};

} // namespace ast
} // namespace mocker

#endif // MOCKER_SCOPE_ID_H
