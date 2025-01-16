#ifndef BUILT_IN_FUNCTIONS_H_   /* Include guard */
#define BUILT_IN_FUNCTIONS_H_

int quit_program();
int pwd();
int cd(char *dir);
int help();
int isBuiltIn(char *funcName);

#endif