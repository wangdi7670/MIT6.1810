#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int read_line(char *new_arg[MAXARG], int cur_num) {
    char buf[512];
    int i = 0;
    while (read(0, buf+i, 1)) {
        if (buf[i] == ' ') {
            buf[i] = '\0';
            new_arg[cur_num] = malloc(strlen(buf) + 1);
            strcpy(new_arg[cur_num], buf);
            cur_num++;
            i = 0;
            continue;
        } else if (buf[i] == '\n') {
            buf[i] = '\0';
            new_arg[cur_num] = malloc(strlen(buf) + 1);
            strcpy(new_arg[cur_num], buf);
            i = 0;
            cur_num++;
            break;
        } else {
            i++;
        }
    
/*         printf("c = %d\n", buf[i]);
        i++; */
    }

    if (i != 0) {
        buf[i + 1] = '\0';
        new_arg[cur_num] = malloc(strlen(buf) + 1);
        strcpy(new_arg[cur_num], buf);
        cur_num++;
    }
    
    return cur_num;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs command (arg ...)\n");
        exit(1);
    }

    char *command = argv[1];
    char *new_arg[MAXARG];
    
    for (int i = 1; i < argc; i++) {
        new_arg[i-1] = argv[i];    
    }

    int init_arg_num = argc - 1;
    int cur_num = init_arg_num;

    while ((cur_num = read_line(new_arg, init_arg_num)) != init_arg_num) {
        new_arg[cur_num] = 0;
        // child process
        if (fork() == 0) {
            exec(command, new_arg);
            exit(0);
        }
        wait(0);
        for (int j = init_arg_num; j < cur_num; j++) {
            free(new_arg[j]);
        }
    }

/*     cur_num = read_line(new_arg, init_arg_num);
    printf("cur_num = %d\n", cur_num);

    new_arg[cur_num] = 0;
    for (int i = 0; i < cur_num; i++)
    {
        printf("new_arg[i] = %s\n", new_arg[i]);
    } */

    exit(0);
}