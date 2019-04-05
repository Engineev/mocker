#ifndef MOCKER_NAIVE_REGISTER_ALLOCATION_H
#define MOCKER_NAIVE_REGISTER_ALLOCATION_H

#include "nasm/module.h"

namespace mocker {

nasm::Module allocateRegistersNaively(const nasm::Module &module);
}

#endif // MOCKER_NAIVE_REGISTER_ALLOCATION_H
