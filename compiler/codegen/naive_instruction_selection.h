#ifndef MOCKER_NAIVE_INSTRUCTION_SELECTION_H
#define MOCKER_NAIVE_INSTRUCTION_SELECTION_H

#include "ir/module.h"
#include "nasm/module.h"

namespace mocker {

nasm::Module runNaiveInstructionSelection(const ir::Module &irModule);
}

#endif // MOCKER_NAIVE_INSTRUCTION_SELECTION_H
