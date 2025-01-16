#include "built_in_functions.h"
#include <unistd.h>
#include <stdio.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
int quit_program();
int pwd();
int cd(char *dir);
int help();
int isBuiltIn(char *funcName);

int isBuiltIn(char *cmd){
    if(strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0){
        return 1;
    }
    else if(strcmp(cmd, "pwd") == 0){
        return 2;
    }
    else if(strcmp(cmd, "cd") == 0){
        return 3;
    }
    else if(strcmp(cmd, "help") == 0){
        return 4;
    }
    else if(strcmp(cmd, "wait") == 0){
        return 5;
    }
    else{
        return -1;
    }
}

int quit_program(){
    exit(0);
    return 0;
}
int pwd(){
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
       printf("%s\n", cwd);
    } 
    else {
       perror("getcwd() error");
       return 1;
   }
   return 0;
}
int cd(char *dir){
    struct stat buffer;   
    bool rValue = (stat (dir, &buffer) == 0);
    if(rValue == true){
        chdir(dir); 
    }
    else{
        printf("Directory does not exist\n");
        return 1;
    }
    return 0;
}
int help(){

    FILE *help = fopen("/home/TU/tup80439/3207/project-2-linux-shell-tup80439/help.txt", "r");
    if (help == NULL) {
        perror("Error opening file: ");
    }
    char fileContents[500];

    while(fgets(fileContents, 100, help)) {
        printf("%s", fileContents);
    }

    printf("\n");
    //puts("cd [dir name]\npwd\nexit\n");
    return 0;
}
