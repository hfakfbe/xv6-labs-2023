#include "kernel/types.h"
#include "user/user.h"

int main(){
    int p1[2], p2[2];
    pipe(p1), pipe(p2);
    int pid = fork();
    if(pid == 0){
        close(p1[1]);
        close(p2[0]);
        char d;
        read(p1[0], &d, 1);
        printf("%d: received ping\n", getpid());
        write(p2[1], &d, 1);
        close(p1[0]);
        close(p2[1]);
    }else if(pid > 0){
        close(p1[0]);
        close(p2[1]);
        char d = 'x';
        write(p1[1], &d, 1);
        read(p2[0], &d, 1);
        printf("%d: received pong\n", getpid());
        close(p1[1]);
        close(p2[0]);
        wait(0);
    }else{
        fprintf(2, "pingpong: error");
        exit(1);
    }
    exit(0);
}
