#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    printf("demo-wd\n");
    // printsb();
    symlink(argv[1], argv[2]);

    /* int fd = open("/temp/temp.txt", O_RDONLY);
    close(fd); */
    return 0;
}