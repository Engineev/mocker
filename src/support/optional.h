// An simplified implementation of std::optional (which was introduced in Cpp17)
// See https://en.cppreference.com/w/cpp/utility/optional for reference

#ifndef MOCKER_OPTIONAL_H
#define MOCKER_OPTIONAL_H

#include <exception>

namespace mocker {

class BadOptionalAccess : public std::exception {};

struct NullOpt_t {};

template <class T> class Optional {
public:
  Optional() = default;

  explicit Optional(NullOpt_t) {}

  Optional(const T & val) : val(new T(val)) {}

  Optional(T && val) : val(new T(std::move(val))) {}

  Optional(const Optional<T> & rhs) { copy(rhs); }

  Optional(Optional<T> && rhs) noexcept { move(std::move(rhs)); }

  Optional<T>& operator=(const Optional<T>& rhs) {
    if (&rhs == this)
      return *this;
    copy(rhs);
    return *this;
  }

  Optional<T>& operator=(Optional<T>&& rhs) noexcept {
    if (&rhs == this)
      return *this;
    move(std::move(rhs));
    return *this;
  }


  ~Optional() {
    delete val;
  }

  const T &value() const & {
    if (val)
      return *val;
    throw BadOptionalAccess();
  }

  template <class U> const T valueOr(U &&defaultValue) const {
    if (val)
      return *val;
    return {std::forward<U>(defaultValue)};
  }

  bool hasValue() const { return val; }

  operator bool() { return val; }

  const T* operator->() const {
    return &operator*();
  }

  T*operator->() {
    return  &operator*();
  }

  const T &operator*() const & { return *val; }

  T &operator*() & { return *val; }

  const T &&operator*() const && { return *val; }

  T &&operator*() && { return *val; }

private:
  T *val = nullptr;

  void copy(const Optional<T>& rhs) {
    if (!rhs) {
      val = nullptr;
      return;
    }
    val = new T(rhs.value());
  }

  void move(Optional<T> && rhs) noexcept {
    val = rhs.val;
    rhs.val = nullptr;
  }

};

} // namespace mocker

#endif // MOCKER_OPTIONAL_H
