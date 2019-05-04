#ifndef MOCKER_DEFER_H
#define MOCKER_DEFER_H

#include <functional>

namespace mocker {

class Defer {
public:
  Defer() = default;
  Defer(std::function<void()> action) : action(std::move(action)) {}
  Defer(const Defer &) = delete;
  Defer(Defer &&o) noexcept {
    action = o.action;
    o.action = nullptr;
  };
  Defer &operator=(const Defer &) = delete;
  Defer &operator=(Defer &&o) noexcept {
    if (this == &o)
      return *this;
    action = o.action;
    o.action = nullptr;
    return *this;
  };

  ~Defer() {
    if (action != nullptr)
      action();
  }

  void execute() {
    action();
    action = nullptr;
  }

private:
  std::function<void()> action = nullptr;
};

} // namespace mocker

#endif // MOCKER_DEFER_H
