#include "kernel/types.h"
#include "user/user.h"

#define MAXN 35

void calc(int p){
    int np[2];
    pipe(np);
    int cur;
    read(p, &cur, 4);
    if(cur == 0){
        close(p);
        exit(0);
    }
    printf("prime %d\n", cur);
    int pid = fork();
    if(pid == 0){
        close(p);
        calc(np[0]);
    }else if(pid > 0){
        while(1){
            int t;
            read(p, &t, 4);
            if(t == 0){
                break;
            }
            if(t % cur){
                write(np[1], &t, 4);
            }
        }
        close(p);
        int t = 0;
        write(np[1], &t, 4);
        close(np[1]);
        wait(0);
    }else{
        fprintf(2, "primes: error\n");
        exit(1);
    }
}

int main(){
    int p[2];
    pipe(p);
    int pid = fork();
    if(pid == 0){
        calc(p[0]);
    }else if(pid > 0){
        for(int i = 2; i <= MAXN; i ++){
            write(p[1], &i, 4);
        }
        int t = 0;
        write(p[1], &t, 4);
        close(p[1]);
        wait(0);
    }else{
        fprintf(2, "primes: error\n");
        exit(1);
    }

    exit(0);
}
