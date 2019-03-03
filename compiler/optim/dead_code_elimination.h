#ifndef MOCKER_DEAD_CODE_ELIMINATION_H
#define MOCKER_DEAD_CODE_ELIMINATION_H

#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "opt_pass.h"

namespace mocker {

class DeadCodeElimination : public FuncPass {
public:
  explicit DeadCodeElimination(ir::FunctionModule &func);

  bool operator()() override;

private:
  bool isParameter(const std::string &identifier);

  void updateWorkList(const std::shared_ptr<ir::IRInst> &inst);

  void init();

  void mark();

  void sweep();

private:
  std::queue<std::string> workList;
  std::unordered_set<ir::InstID> useful;
  std::unordered_map<std::string, std::shared_ptr<ir::IRInst>> instDefine;
  std::size_t cnt = 0;
};

} // namespace mocker

#endif // MOCKER_DEAD_CODE_ELIMINATION_H
