#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("demo: wrong args\n");
        exit(1);
    }
    int n = atoi(argv[1]);
    demo(n);
    return 0;
}