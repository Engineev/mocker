#ifndef MOCKER_SYM_TBL_H
#define MOCKER_SYM_TBL_H

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast_node.h"
#include "scope_id.h"
#include "support/optional.h"
#include "visitor.h"

namespace mocker {
namespace ast {

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
  std::shared_ptr<Declaration> lookUp(const ScopeID &scopeID,
                                      const std::string &identifier);

  // If the identifier has already exists in the CURRENT scope, no addition will
  // be perform and the return value is false. Otherwise add the symbol into the
  // current scope, symbols from outer scopes may be covered, and return true.
  bool addSymbol(const ScopeID &scopeID, const std::string &identifier,
                 std::shared_ptr<Declaration> decl);

  void print() const;

private:
  struct Scope {
    explicit Scope(const std::shared_ptr<Scope> &pnt);

    std::unordered_map<std::string, std::shared_ptr<Declaration>> syms;
    std::vector<std::shared_ptr<Scope>> subscopes;
    std::weak_ptr<Scope> pnt;
  };

  // Just a helper function. Some asserts are performed.
  std::shared_ptr<Scope> getScope(const ScopeID &id);

  void printImpl(const std::string &scopeName,
                 const std::shared_ptr<Scope> &cur) const;

  std::shared_ptr<Scope> root;
};

} // namespace ast
} // namespace mocker

#endif // MOCKER_SYM_TBL_H
