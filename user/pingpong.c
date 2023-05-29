#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SIZE 4

int main(int argc, char *argv[]) {
    int fd[2];
    pipe(fd);

    char *ping = "ping";
    char *pong = "pong";

    char inbuf[SIZE + 1];
    inbuf[SIZE] = '\0';

    if (fork() == 0) {
        read(fd[0], inbuf, SIZE);
        int pid = getpid();
        printf("%d: received %s\n", pid, inbuf);
        write(fd[1], pong, SIZE);
        exit(0);
    } else {
        write(fd[1], ping, SIZE);
        wait((void *) 0);
        read(fd[0], inbuf, SIZE);
        int pid = getpid();
        printf("%d: received %s\n", pid, inbuf);
        exit(0);
    }
}