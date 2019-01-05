# Convention

## Renaming

During the compilation, the identifiers of variables are renamed, which is done
by invoking SemanticChecker::renameIdentifiers after the semantic check. The 
rules of renaming are listed here for reference.

* For global variables, an '@' is added at the beginning.
* For member variables, a '#' is added at the beginning.
* For local variables, the scope information is encoded into their identifiers.
  The form of encoding is not specified but it is guaranteed that identifiers of 
  variables of different scopes are different. The identifiers after renaming
  are of form \<ScopeInfo>\_\<OriginalIdentifier>, where ScopeInfo consists of
  only alphas, digits and '-'.


## Constructors

The constructors of all user-defined types will be named in form 
\_ctor\_\<TypeIdentifier> and the return type of which is void. In consequence,
special handling is required when performing semantic check. If a constructor
is defined explicitly, then the naming process will be carried out during 
parsing, otherwise, a default constructor will be injected during the semantic
check.
