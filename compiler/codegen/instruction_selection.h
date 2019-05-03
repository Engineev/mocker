#ifndef MOCKER_INSTRUCTION_SELECTION_H
#define MOCKER_INSTRUCTION_SELECTION_H

#include "ir/module.h"
#include "nasm/module.h"

namespace mocker {

nasm::Module runInstructionSelection(const ir::Module &irModule);
}

#endif // MOCKER_INSTRUCTION_SELECTION_H
