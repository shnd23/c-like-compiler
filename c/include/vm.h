#ifndef MINIC_VM_H
#define MINIC_VM_H

#include <stdio.h>

#include "bytecode.h"

#define VM_DEFAULT_MAX_STEPS 10000000LL

typedef struct {
    long long steps;
    const char *fault;
} VmResult;

VmResult vm_run(const Bytecode *program, FILE *out, long long max_steps);

#endif
