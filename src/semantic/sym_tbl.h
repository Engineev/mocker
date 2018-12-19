#ifndef MOCKER_SYM_TBL_H
#define MOCKER_SYM_TBL_H

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast/ast_node.h"
#include "ast/visitor.h"

namespace mocker {

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
  bool operator!=(const ScopeID & rhs) const {
    return !(*this == rhs);
  }

private:
  friend class SymTbl;
  ScopeID(std::vector<std::size_t> ids) : ids(std::move(ids)) {}

  std::vector<std::size_t> ids;
};

class SymTbl {
public:
  SymTbl();
  SymTbl(const SymTbl &) = delete;
  SymTbl(SymTbl &&) = default;

  // return the scope ID of the global scope
  ScopeID global() const;

  // Create a subscope in the scope provided and return the ID of the new scope.
  // If the scope provided does not exists, throw std::out_of_range
  ScopeID createSubscope(const ScopeID &pntID);

  // Try to find the given symbol in the given scope AND it parent scopes.
  // If the symbol does not exist, then the returned shared_ptr is empty.
  std::shared_ptr<ast::Declaration> lookUp(const ScopeID &scopeID,
                                      const std::string &identifier);

  // If the identifier has already exists in the CURRENT scope, no addition will
  // be perform and the return value is false. Otherwise add the symbol into the
  // current scope, symbols from outer scopes may be covered, and return true.
  bool addSymbol(const ScopeID &scopeID, const std::string &identifier,
                 std::shared_ptr<ast::Declaration> decl);

private:
  struct Scope {
    explicit Scope(const std::shared_ptr<Scope> &pnt);

    std::unordered_map<std::string, std::shared_ptr<ast::Declaration>> syms;
    std::vector<std::shared_ptr<Scope>> subscopes;
    std::weak_ptr<Scope> pnt;
  };

  // Just a helper function. Some asserts are performed.
  std::shared_ptr<Scope> getScope(const ScopeID &id);

  std::shared_ptr<Scope> root;
};

} // namespace mocker

#endif // MOCKER_SYM_TBL_H
