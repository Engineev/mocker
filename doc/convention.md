# Convention

## Renaming

During the compilation, the identifiers of variables and methods are renamed. 
The process is carried out by invoking SemanticChecker::renameIdentifiers after
the semantic check and the rules of renaming are listed here for reference.

* For global variables, an '@' will be added at the beginning.
* For member variables, #\<TypeName\># will be added at the beginning. For 
  example, suppose in `class A` there is an `int a`, then every identifier which
  refers to it will be renamed to `#A#a`.
* For member functions (aka. methods), #\<TypeName\># will be added at the
  beginning.
* For local variables, the scope information is encoded into their identifiers.
  The form of encoding is not specified but it is guaranteed that identifiers of 
  variables of different scopes are different. The identifiers after renaming
  are of form \<ScopeInfo>\_\<OriginalIdentifier>, where ScopeInfo consists of
  only alphas, digits and '-'.


## Constructors

The constructors of all user-defined types will be named \_ctor\_ and the return
type of which is void. As a consequence, special handling is required when 
performing semantic check. If a constructor is defined explicitly, then the 
naming process will be carried out during parsing, otherwise, a default 
constructor will be injected during the semantic check. Note that a constructor
is also a member function and therefore will be renamed. See the previous
section for details.
