#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int isFile(const char  *fileName);
int isDir(const char *fileName);
bool file_exists (const char *filename);
bool isExec(const char *fileName);

int isFile(const char  *fileName){
	struct stat sb;
	stat(fileName, &sb);
	if (stat(fileName, &sb) != 0) {
        perror("stat failed");
        return 1;
    }
	else if(S_ISREG(sb.st_mode) > 0){
		//puts("this is a file");
	  	return 0;
	}
	return 1;
}

int isDir(const char *fileName){
    struct stat sb;
	stat(fileName, &sb);
	if (stat(fileName, &sb) != 0) {
        perror("stat failed");
        return 1;
    }
	else if (S_ISDIR(sb.st_mode) > 0) {
		//puts("this is a dir");
	 	return 0;
	} 
	else 
	 	return 1;	
}
bool file_exists (const char *filename) {
  struct stat   buffer;   
  return (stat (filename, &buffer) == 0);
}
bool isExec(const char *fileName){
	struct stat sb;
	if (stat(fileName, &sb) == 0 && sb.st_mode & S_IXUSR) {
		/* executable */
		return true;
	}

	else  {
		/* non-executable */
		return false;
	}

}