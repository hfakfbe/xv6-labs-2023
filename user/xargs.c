#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

int emptychar(char c){
    return c == '\n' || c == ' ' || c == '\t' || c == '\r' || c == '\f';
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("xargs: too few argument\n");
        exit(0);
    }

    char *buf[16];
    memset(buf, 0, sizeof(buf));
    for(int i = 1; i < argc; i ++){
        buf[i - 1] = argv[i];
    }

    while(1){
        int cnt = argc - 2, p = 0, ok = 0;
        char tmp;
        while((ok = read(0, &tmp, 1)) == 1 && tmp != '\n'){
            if(p == 0 && !emptychar(tmp)){
                cnt ++;
                buf[cnt] = malloc(sizeof(char) * 32);
                buf[cnt][p ++] = tmp;
                buf[cnt][p] = '\0';
            }else if(emptychar(tmp)){
                p = 0;
            }else{
                buf[cnt][p ++] = tmp;
                buf[cnt][p] = '\0';
            }
        }
        if(ok != 1){
            break;
        }
        int pid = fork();
        if(pid == 0){
            exec(buf[0], buf);
            exit(1);
        }else if(pid < 0){
            // for(int i = 0; buf[i] != 0; i ++){
            //     printf(" %d: %s\n", i, buf[i]);
            // }
            fprintf(2, "xargs: fork %s fail\n", buf[0]);
            exit(1);
        }

        wait(0);
        for(int i = argc - 1; i <= cnt; i ++){
            free(buf[i]);
        }
    }
    
    exit(0);
}