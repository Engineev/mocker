#ifndef MOCKER_LOCAL_VALUE_NUMBERING_H
#define MOCKER_LOCAL_VALUE_NUMBERING_H

#include "opt_pass.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace mocker {

class LocalValueNumbering : public BasicBlockPass {
public:
  explicit LocalValueNumbering(ir::BasicBlock &bb);

  bool operator()() override;

private:
  std::string hash(const std::shared_ptr<ir::Addr> &addr);

  void makeSureDefined(const std::shared_ptr<ir::Addr> &addr);

  std::string hash(const std::shared_ptr<ir::IRInst> &inst,
                   const std::vector<std::size_t> &valueNumbers);

private:
  std::unordered_map<std::string, std::size_t> key2ValueNumber;
  std::vector<std::shared_ptr<ir::Addr>> valueNumber2Addr;
};

} // namespace mocker

#endif //MOCKER_LOCAL_VALUE_NUMBERING_H
