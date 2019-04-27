#ifndef MOCKER_LIVENESS_H
#define MOCKER_LIVENESS_H

#include <unordered_set>
#include <utility>

#include "helper.h"
#include "nasm/inst.h"
#include "nasm_cfg.h"

namespace mocker {

// For each block b, LiveOut(b) is the collections of variables that are live
// at the end of b.
std::unordered_map<std::string, nasm::RegSet> buildLiveOut(const NasmCfg &cfg);

} // namespace mocker

#endif // MOCKER_LIVENESS_H
