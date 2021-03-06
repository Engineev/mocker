#include "sym_tbl.h"

#include <cassert>
#include <iostream>

#include "ast/helper.h"

namespace mocker {

SymTbl::Scope::Scope(const std::shared_ptr<SymTbl::Scope> &pnt) : pnt(pnt) {}

SymTbl::SymTbl() : root(std::make_shared<Scope>(nullptr)) {}

ScopeID SymTbl::global() const { return ScopeID(); }

ScopeID SymTbl::createSubscope(const ScopeID &pntID) {
  auto pnt = getScope(pntID);
  pnt->subscopes.emplace_back(std::make_shared<Scope>(pnt));
  auto id = pntID.ids;
  id.emplace_back(pnt->subscopes.size() - 1);
  return ScopeID(std::move(id));
}

std::shared_ptr<SymTbl::Scope> SymTbl::getScope(const ScopeID &ids) const {
  auto res = root;
  for (const std::size_t i : ids.ids) {
    if (i >= res->subscopes.size())
      throw std::out_of_range("Scope required does not exist");
    res = res->subscopes[i];
  }
  return res;
}

std::shared_ptr<ast::Declaration>
SymTbl::lookUp(const ScopeID &scopeID, const std::string &identifier) const {
  auto cur = getScope(scopeID);
  while (cur != nullptr) {
    auto iter = cur->syms.find(identifier);
    if (iter != cur->syms.end())
      return {iter->second};
    cur = cur->pnt.lock();
  }
  return {};
}

bool SymTbl::addSymbol(const ScopeID &scopeID, const std::string &identifier,
                       std::shared_ptr<ast::Declaration> decl) {
  auto scope = getScope(scopeID);
  if (scope->syms.find(identifier) != scope->syms.end())
    return false;
  scope->syms.emplace(identifier, std::move(decl));
  return true;
}

} // namespace mocker