#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


// 参考Leetcode 第204题
int main() {
    int low = 2;
    int high = 35;
    
    int arr[high + 1];
    for (int i = low; i <= high; i++) {    
        arr[i] = 1;
    }

    for (int i = 2; i <= high; i++) {
        if (arr[i] == 1) {
            for (int j = i * i; j <= high; j += i) {
                arr[j] = 0;
            }
        }
    }

    for (int i = 2; i <= 35; i++) {
        if (arr[i] == 1) {
            printf("prime %d\n", i);
        }
    }

    exit(0);
}