#ifndef MOCKER_DEFER_H
#define MOCKER_DEFER_H

#include <functional>

namespace mocker {

class Defer {
public:
  Defer(std::function<void()> action) : action(std::move(action)) {}

  Defer(const Defer &) = default;
  Defer(Defer &&) noexcept = default;
  Defer &operator=(const Defer &) = default;
  Defer &operator=(Defer &&) noexcept = default;

  ~Defer() { action(); }

private:
  std::function<void()> action;
};

} // namespace mocker

#endif // MOCKER_DEFER_H
