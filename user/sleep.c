#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    if(argc <= 1){
        fprintf(2, "sleep: too few argument\n");
        exit(0);
    }
    int n = atoi(argv[1]);
    sleep(n);
    exit(0);
}
