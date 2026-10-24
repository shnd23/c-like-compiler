#ifndef MINIC_OPTIMIZER_H
#define MINIC_OPTIMIZER_H

#include "ir.h"

void optimize_code(Arena *arena, const InstrList *input, size_t var_count, InstrList *output);

#endif
