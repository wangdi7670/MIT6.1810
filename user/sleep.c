#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("argc is wrong\n");
        exit(1);
    }

    int time = atoi(argv[1]);
    sleep(time);
    exit(0);
    
}