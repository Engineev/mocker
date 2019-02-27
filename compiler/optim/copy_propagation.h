#ifndef MOCKER_COPY_PROPAGATION_H
#define MOCKER_COPY_PROPAGATION_H

#include "opt_pass.h"

#include <unordered_map>

namespace mocker {

class CopyPropagation : public FuncPass {
public:
  explicit CopyPropagation(ir::FunctionModule &func);

  void operator()() override;

private:
  void buildValue();

  void rewrite();

private:
  // The value to propagate
  std::unordered_map<std::string, std::shared_ptr<ir::Addr>> value;
};

} // namespace mocker

#endif // MOCKER_COPY_PROPAGATION_H
