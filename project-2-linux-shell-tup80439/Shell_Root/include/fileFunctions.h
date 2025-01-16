#ifndef FILE_FUNCTIONS_H_   /* Include guard */
#define FILE_FUNCTIONS_H_
#include <stdbool.h>


int isFile(const char  *fileName);
int isDir(const char *fileName);
bool file_exists (const char *filename);
bool isExec(const char *fileName);

#endif