#ifndef MOCKER_PEEPHOLE_H
#define MOCKER_PEEPHOLE_H

#include "nasm/module.h"

namespace mocker {

nasm::Module runPeepholeOptimization(const nasm::Module &module);
}

#endif // MOCKER_PEEPHOLE_H
