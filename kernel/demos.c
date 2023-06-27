#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

void demo1() {
    printf("Result: %d\n", sum_to(5));
}

void demo2() {
    printf("Result: %d\n", sum_then_double(5));
}