#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

// https://zhuanlan.zhihu.com/p/567525198 调试用户程序教程

void findfile(char *path, char *filename){
    int fd;
    // for(int i = 0; i < strlen(path); i ++){
    //     printf("%x(%c) ", path[i], path[i]);
    // }
    // printf("*\n");
    if((fd = open(path, O_RDONLY)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    struct dirent de;
    struct stat st; 
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        return;
    }
    if(st.type != T_DIR){
        close(fd);
        return;
    }

    char *p = path + strlen(path);
    *p ++ = '/';
    int len = strlen(filename);
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0){
            continue;
        }
        if(strcmp(filename, de.name) == 0){
            memcpy(p, filename, len);
            *(p + strlen(de.name)) = '\0';
            printf("%s\n", path);
        }
    }
    close(fd);

    *(p - 1) = '\0';
    if((fd = open(path, O_RDONLY)) < 0){
        fprintf(2, "find: cannot open the second %s\n", path);
        return;
    }
    *(p - 1) = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0){
            continue;
        }
        if(strcmp(".", de.name) == 0 || strcmp("..", de.name) == 0){
            continue;
        }
        memcpy(p, de.name, strlen(de.name));
        *(p + strlen(de.name)) = '\0';
        // printf("this is %s, filename %s\n", path, de.name);
        findfile(path, filename);
    }
    close(fd);
}

int main(int argc, char *argv[]){
    if(argc < 3){
        fprintf(2, "find: too few argument\n");
        exit(1);
    }
    char buf[512];
    memcpy(buf, argv[1], strlen(argv[1]));
    findfile(buf, argv[2]);
}