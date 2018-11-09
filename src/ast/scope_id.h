#ifndef MOCKER_SCOPE_ID_H
#define MOCKER_SCOPE_ID_H

#include <vector>
#include <functional>
#include <cstddef>

namespace mocker {
namespace ast {

class ScopeID {
public:
  ScopeID() = default;
  ScopeID(const ScopeID &) = default;
  ScopeID(ScopeID &&) noexcept = default;
  ScopeID& operator=(const ScopeID&) = default;
  ScopeID& operator=(ScopeID &&) noexcept = default;
  ~ScopeID() = default;

private:
  friend class SymTbl;
  ScopeID(std::vector<std::size_t> ids) : ids(std::move(ids)) {}

  std::vector<std::size_t> ids;
};

}
}

#endif //MOCKER_SCOPE_ID_H
