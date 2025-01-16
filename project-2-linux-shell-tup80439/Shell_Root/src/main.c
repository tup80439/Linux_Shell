/*
    This program was built by Dylan Brown in September of 2024. It tries to mimic the bash shell on the linux server. It does it
    by using 4 built in functions and calling the rest by searching the enviroment path.

    Modified to add background process support and 'wait' built-in command.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include "fileFunctions.h"
#include "built_in_functions.h"
#include "helpers.h"
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <asm-generic/signal-defs.h>


#define MAX_BG_PROCESSES 100  // Maximum number of background processes

int pipeHelper(int numPipes, char* parsedArray[], bool isBackground);
bool wePiping(char* parsedArray[]);
char** parse(char* line, const char delim[]);
int funcCaller(char **parsedText, int num);
char *correctedInput(char *cmd);
bool weRedirecting(char* parsedArray[]);
int redirectHelper(char* parsedArray[], bool isBackground);
void stripQuotes(char *str);
void waitForBackgroundProcesses();
void sigchld_handler(int signum);

// Global variables to keep track of background processes
pid_t bg_pids[MAX_BG_PROCESSES];
int num_bg_pids = 0;

int main(int argc, char* argv[]){
    // Install SIGCHLD handler
    struct sigaction sa;
    sa.sa_handler = &sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // Restart interrupted syscalls, don't catch stopped children
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    char cmd[256];
    while(1){

        printf("tush$ ");
        fflush(stdout);  // Ensure the prompt is displayed immediately

        if (fgets(cmd, 256, stdin) == NULL) {
            if (errno == EINTR) {
                // Interrupted by signal, restart fgets
                clearerr(stdin);  // Clear error flags
                continue;
        } 
            else {
                // End of input (e.g., Ctrl+D)
                break;
            }
        }
        if(strcmp(cmd,"\n") == 0){
            continue;
        }
        char **cmdParsed = parse(cmd, " \n");
        for (int i = 0; cmdParsed[i] != NULL; i++) {
            stripQuotes(cmdParsed[i]);
        }

        // Count tokens and check for '&' at the end
        int numTokens = 0;
        while(cmdParsed[numTokens] != NULL){
            numTokens++;
        }

        bool isBackground = false;
        if(numTokens > 0 && strcmp(cmdParsed[numTokens - 1], "&") == 0){
            isBackground = true;
            cmdParsed[numTokens -1 ] = NULL;  // Remove '&' from tokens
            numTokens--;
        }

        int funcNum = isBuiltIn(cmdParsed[0]);
        if(funcNum != -1){ // checks if it's built in then calls that function
            if(isBackground){
                printf("Built-in commands cannot be run in the background.\n");
            } else {
                funcCaller(cmdParsed, funcNum);
            }
            free(cmdParsed);
        }
        else if(wePiping(cmdParsed)){
            // Handle piping
            pipeHelper(1, cmdParsed, isBackground);
            free(cmdParsed);
        }
        else if(weRedirecting(cmdParsed)){
            // Handle redirection
            redirectHelper(cmdParsed, isBackground);
            free(cmdParsed);
        }
        else{
            // Handle normal commands
            char cmdPath[256];
            if(correctedInput(cmdParsed[0]) == NULL){
                printf("Command '%s' not found\n", cmdParsed[0]);
                free(cmdParsed);
                continue;
            }
            strcpy(cmdPath, correctedInput(cmdParsed[0]));
            int pid = fork();
            if(pid == 0){
                execv(cmdPath, cmdParsed);
                perror("execv failed");
                exit(1);
            }
            if(pid > 0){
                if(isBackground){
                    // Background process
                    printf("[%d] %d\n", num_bg_pids + 1, pid); // Print job number and PID
                    // Add pid to bg_pids
                    if(num_bg_pids < MAX_BG_PROCESSES){
                        bg_pids[num_bg_pids++] = pid;
                    } else {
                        printf("Maximum background processes reached.\n");
                    }
                } else {
                    // Foreground process
                    waitpid(pid, NULL, 0);
                }
            }
            free(cmdParsed);
        }
    }
}

int funcCaller(char **parsedText, int num){ // returns -1 if not built in
    if(num == 1){
        quit_program();
    }
    else if(num == 2){
        pwd();
    }
    else if(num == 3){
        cd(parsedText[1]);
    }
    else if(num == 4){
        help();
    }
    else if(num == 5){
        waitForBackgroundProcesses();
    }
    else{
        return -1;
    }
    return 0;
}

void waitForBackgroundProcesses(){
    for(int i = 0; i < num_bg_pids; i++){
        int status;
        waitpid(bg_pids[i], &status, 0);
        printf("Process %d finished\n", bg_pids[i]);
    }
    num_bg_pids = 0;
}


char *correctedInput(char *cmd){
    /* Find the command */
    char* envPath = getenv("PATH");
    char envPath_cpy[512];
    strcpy(envPath_cpy,envPath);
    char filePath[512];
    strncpy(envPath_cpy, envPath, sizeof(envPath_cpy) - 1);
    envPath_cpy[sizeof(envPath_cpy) - 1] = '\0';  

    //checks if the file already has a path
    for (size_t i = 0; i < strlen(cmd); i++){
        if(cmd[i] == '/'){
            if(isExec(cmd)){
                return cmd;
            }
            else{
                return NULL;
            }
        }
    }
    
    char* pch = strtok(envPath_cpy, ":");
    while(pch != NULL){
        snprintf(filePath, 512, "%s/%s", pch,cmd);
        if(isExec(filePath)){
            return strdup(filePath); 
        }
        pch = strtok(NULL, ":");
    }

    /* If null is returned then the command was never found */
    return NULL;
}

int pipeHelper(int numPipes, char* parsedArray[], bool isBackground) {
    // Parse commands
    int numCmds = 1;
    for(int i = 0; parsedArray[i] != NULL; i++){
        if(strcmp(parsedArray[i], "|") == 0){
            numCmds++;
        }
    }

    char ***cmds = malloc(sizeof(char**) * numCmds);
    int cmdIndex = 0, argIndex = 0;
    while(cmdIndex < numCmds){
        int cmdArgCount = 0;
        int start = argIndex;
        while(parsedArray[argIndex] != NULL && strcmp(parsedArray[argIndex], "|") != 0){
            cmdArgCount++;
            argIndex++;
        }
        cmds[cmdIndex] = malloc(sizeof(char*) * (cmdArgCount + 1));
        for(int i = 0; i < cmdArgCount; i++){
            cmds[cmdIndex][i] = parsedArray[start + i];
        }
        cmds[cmdIndex][cmdArgCount] = NULL;
        cmdIndex++;
        if(parsedArray[argIndex] != NULL){
            argIndex++;
        }
    }

    // Check for output redirection in last command
    int outputRedirect = 0; // 1 for '>', 2 for '>>'
    char *outputFile = NULL;
    char **lastCmdArgs = cmds[numCmds - 1];
    int lastCmdArgc = 0;
    while(lastCmdArgs[lastCmdArgc] != NULL){
        lastCmdArgc++;
    }
    char **newLastCmdArgs = malloc(sizeof(char*) * (lastCmdArgc + 1));
    int newArgc = 0;
    for(int i = 0; lastCmdArgs[i] != NULL; i++){
        if(strcmp(lastCmdArgs[i], ">") == 0){
            outputRedirect = 1;
            outputFile = lastCmdArgs[++i];
        }
        else if(strcmp(lastCmdArgs[i], ">>") == 0){
            outputRedirect = 2;
            outputFile = lastCmdArgs[++i];
        }
        else{
            newLastCmdArgs[newArgc++] = lastCmdArgs[i];
        }
    }
    newLastCmdArgs[newArgc] = NULL;
    cmds[numCmds -1 ] = newLastCmdArgs;

    // Create pipes
    int numPipesActual = numCmds -1;
    int pipes[numPipesActual][2];
    for(int i = 0; i < numPipesActual; i++){
        pipe(pipes[i]);
    }

    // Array to store child PIDs
    int child_pids[numCmds];

    // Fork processes
    for(int i = 0; i < numCmds; i++){
        int pid = fork();
        if(pid == 0){
            if(i > 0){
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if(i < numCmds -1){
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Output redirection in last command
            if(i == numCmds -1 && outputRedirect){
                if(outputRedirect ==1){
                    freopen(outputFile, "w", stdout);
                }
                else if(outputRedirect ==2){
                    freopen(outputFile, "a", stdout);
                }
            }
            for(int j = 0; j < numPipesActual; j++){
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            char *cmd_path = correctedInput(cmds[i][0]);
            execv(cmd_path, cmds[i]);
            exit(1);
        }
        else{
            child_pids[i] = pid;  // Store child PID
        }
    }

    // Parent process closes pipes
    for(int i = 0; i < numPipesActual; i++){
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if(!isBackground){
        // Wait for child processes
        for(int i = 0; i < numCmds; i++){
            waitpid(child_pids[i], NULL, 0);
        }
    } else {
        // Background process, do not wait
        // Add child PIDs to bg_pids
        for(int i = 0; i < numCmds; i++){
            if(num_bg_pids < MAX_BG_PROCESSES){
                bg_pids[num_bg_pids++] = child_pids[i];
            } else {
                printf("Maximum background processes reached.\n");
                break;
            }
        }
        printf("Background job started with PIDs: ");
        for(int i = 0; i < numCmds; i++){
            printf("%d ", child_pids[i]);
        }
        printf("\n");
    }

    // Free allocated memory
    for(int i = 0; i < numCmds; i++){
        free(cmds[i]);
    }
    free(cmds);

    return 0;
}

bool wePiping(char* parsedArray[]) {
    size_t i = 0;

    // Loop through until we hit the NULL element
    while (parsedArray[i] != NULL) {
        // Check if the current element is "|"
        if (strcmp(parsedArray[i], "|") == 0) {
            // WE PIPING FR SMH
            return true;
        }
        i++;
    }

    // WE IS NOT PIPING JACK
    return false;
}

bool weRedirecting(char* parsedArray[]) {
    for (int i = 0; parsedArray[i] != NULL; i++) {
        if (strcmp(parsedArray[i], "<") == 0 || strcmp(parsedArray[i], ">") == 0 || strcmp(parsedArray[i], ">>") == 0 || strcmp(parsedArray[i], "<<") == 0 ) {
            return true;
        }
    }
    return false;
}

int redirectHelper(char* parsedArray[], bool isBackground) {
    int inputRedirect = 0;  // 1 for '<', 2 for '<<'
    int outputRedirect = 0; // 1 for '>', 2 for '>>'
    char *inputFile = NULL;
    char *outputFile = NULL;
    char *hereDocumentDelimiter = NULL; // For '<<'

    // Prepare the command arguments
    char **cmdArgs = malloc(sizeof(char*) * 256);
    int argIndex = 0;

    for(int i = 0; parsedArray[i] != NULL; i++){
        if(strcmp(parsedArray[i], "<") == 0){
            inputRedirect = 1;
            inputFile = parsedArray[++i];
        }
        else if(strcmp(parsedArray[i], "<<") == 0){
            inputRedirect = 2;
            hereDocumentDelimiter = parsedArray[++i];
        }
        else if(strcmp(parsedArray[i], ">") == 0){
            outputRedirect = 1;
            outputFile = parsedArray[++i];
        }
        else if(strcmp(parsedArray[i], ">>") == 0){
            outputRedirect = 2;
            outputFile = parsedArray[++i];
        }
        else{
            cmdArgs[argIndex++] = parsedArray[i];
        }
    }
    cmdArgs[argIndex] = NULL;

    int pid = fork();
    if(pid == 0){
        if(inputRedirect == 1){
            freopen(inputFile, "r", stdin);
        }
        else if(inputRedirect == 2){
            // Here-document handling
            char tmpFilename[] = "/tmp/tush_tmpfile_XXXXXX";
            int tmpfd = mkstemp(tmpFilename);
            FILE *tmpFile = fdopen(tmpfd, "w+");
            char line[1024];
            printf("heredoc> ");
            while(fgets(line, sizeof(line), stdin)){
                if(strncmp(line, hereDocumentDelimiter, strlen(hereDocumentDelimiter)) == 0)
                    break;
                fputs(line, tmpFile);
                printf("heredoc> ");
            }
            fflush(tmpFile);
            fseek(tmpFile, 0, SEEK_SET);
            dup2(tmpfd, STDIN_FILENO);
            fclose(tmpFile);
            unlink(tmpFilename);
        }
        if(outputRedirect == 1){
            freopen(outputFile, "w", stdout);
        }
        else if(outputRedirect == 2){
            freopen(outputFile, "a", stdout);
        }
        char *cmdPath = correctedInput(cmdArgs[0]);
        execv(cmdPath, cmdArgs);
        exit(1);
    }
    else if(pid > 0){
        if(isBackground){
            // Background process
            if(num_bg_pids < MAX_BG_PROCESSES){
                bg_pids[num_bg_pids++] = pid;
                printf("Background job started with PID: %d\n", pid);
            } else {
                printf("Maximum background processes reached.\n");
            }
        } else {
            waitpid(pid, NULL, 0);
        }
    }

    free(cmdArgs);
    return 0;
}

void stripQuotes(char *str) {
    size_t len = strlen(str);
    if (len >= 2) {
        if ((str[0] == '"' && str[len - 1] == '"') ||
            (str[0] == '\'' && str[len - 1] == '\'')) {
            // Shift the string to the left by one character
            memmove(str, str + 1, len - 2);
            str[len - 2] = '\0'; // Null-terminate the string
        }
    }
}

void sigchld_handler(int signum) {
    int saved_errno = errno;  // Save errno
    pid_t pid;
    int status;

    // Reap all terminated child processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Remove pid from bg_pids
        for(int i = 0; i < num_bg_pids; i++){
            if(bg_pids[i] == pid){
                // Shift the rest of the array
                for(int j = i; j < num_bg_pids - 1; j++){
                    bg_pids[j] = bg_pids[j + 1];
                }
                num_bg_pids--;
                break;
            }
        }
        //printf("\nBackground process %d finished\n", pid);
        // Do not reprint the prompt here
    }
    errno = saved_errno;  // Restore errno
}
