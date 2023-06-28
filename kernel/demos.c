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

int dummymain(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        printf("Argument %d: %s\n", i, argv[i]);
    }

    return 0;
}

void demo4() {
    char *args[] = {"foo", "bar", "baz"};
    int res = dummymain(sizeof(args)/sizeof(args[0]), args); 
    if (res < 0) {
        panic("demo 4");
    }
}