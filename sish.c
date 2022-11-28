#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
By Ruoxian Weng, completed on Mar 24, 2022.
This program is an implementation of simple shell in Unix/Linux.
The shell waits for user input. As a command is typed, the shell executes it through its child process. When the execution
is completed, it waits for input again until user enters "exit" (terminate the shell).
*/

//Global variables
const int MAX_SIZE = 10000;
int commandLineCount = 0, position = 0, pipeCount = 0, argCount = 0; //shared variables
char *commandLines[100]; //store the latest 100 command lines

//Prototypes
void storeCommandLine(char str[]);
void parseLine(char *s, char *args[], int fd[][2]);
void executePiped(int fd[][2], char *args[]);
void execute(int fd[][2], char *args[], char str[]);
void shellExit(char *s);
void cd(char *dir);
void history(char *arg);
void executeHistory(char *args[], int fd[][2], char str[]);
int isNumber(char *s);

int main(int argc, char *argv[]) {
        char *string = NULL;
        char str[MAX_SIZE];
        int charRead, i;
        size_t size;
        char *args[100]; //store command arguments
        int fd[100][2]; //store read/write file descriptors returned by each pipe

        //allocate memory for each string in commandLines[]
        for (i = 0; i < 100; i++) {
                commandLines[i] = (char*) malloc(MAX_SIZE*sizeof(char));
        }

        while (1) {
                printf("sish> ");

                //get string from stdin
                charRead = getline(&string, &size, stdin);
                if (charRead == -1) {
                        perror("fail to read a line");
                }

                //remove '\n' at the end of string
                string += charRead - 1;
                if (*string == '\n') {
                        *string = '\0';
                }
                string -= charRead - 1;

                //exit command
                shellExit(string);

                //backup for string
                strcpy(str, string);

                //parse string and execute piped command
                parseLine(string, args, fd);

                //no command entered (whitespace or newline)
                if (args[0] == NULL) {
                        continue; //ask for input again
                }

                if (strcmp(args[0], "history") != 0) { //command != "history"
                        storeCommandLine(str);
                }
                execute(fd, args, str);

                //close all pipes for parent
                for (i = 0; i < pipeCount; i++) {
                        close(fd[i][0]);
                        close(fd[i][1]);
                }

                //wait will return -1 when all child processes terminated
                while (wait(NULL) > 0);

                pipeCount = 0;
                argCount = 0;
                args[0] = NULL;
        }
}

//store command lines to memory
void storeCommandLine(char str[]) {
        if (strcmp(str, "") == 0) { //str is empty
                return;
        }

        strcpy(commandLines[position], str);
        commandLineCount++;
        position++;
        if (commandLineCount % 100 == 0) {
                position = 0;
        }
}

//parse a line into arguments
//if it is piped, execute the piped commands
void parseLine(char *s, char *args[], int fd[][2]) {
        char *token, *saveptr1, *saveptr2;

        token = strtok_r(s, "|", &saveptr1);
        while (token != NULL) {
                //store command to args
                args[argCount] = strtok_r(token, " ", &saveptr2);
                while (args[argCount] != NULL) {
                        argCount++;
                        args[argCount] = strtok_r(NULL, " ", &saveptr2);
                }

                token = strtok_r(NULL, "|", &saveptr1);
                if (token != NULL) { //piped command (not last command)
                        args[argCount+1] = NULL;
                        //create pipe
                        if (pipe(fd[pipeCount]) == -1) {
                                perror("fail to create pipe");
                        }
                        executePiped(fd, args);
                        argCount = 0;
                        pipeCount++;
                }
        }
}

//execute not last piped commands
void executePiped(int fd[][2], char *args[]) {
        pid_t cpid = fork();
        if (cpid < 0) {
                perror("fail to create child process");
        }
        if (cpid == 0) { //only child goes here
                if (pipeCount != 0) { //not first command
                        //read from previous pipe
                        if (dup2(fd[pipeCount-1][0], STDIN_FILENO) == -1) {
                                perror("dup2 failed");
                        }
                }
                //write to current pipe
                if (dup2(fd[pipeCount][1], STDOUT_FILENO) == -1) {
                        perror("dup2 failed");
                }

                //close pipes
                int j;
                for (j = 0; j <= pipeCount; j++) {
                        close(fd[j][0]);
                        close(fd[j][1]);
                }

                //execute command
                execvp(args[0], args);
                perror("execvp failed");
                exit(EXIT_SUCCESS);
        }
}

//execute nonpiped commands (including buildin commands) and/or last piped command
void execute(int fd[][2], char *args[], char str[]) {
        //"cd" command
        if (strcmp(args[0], "cd") == 0) {
                if (argCount > 2) {
                        perror("too many arguments for cd");
                        return;
                }
                else {
                        cd(args[1]);
                }
        }
        //"history" command
        else if (strcmp(args[0], "history") == 0) {
                if (argCount > 2) {
                        perror("too many arguments for history");
                        storeCommandLine(str);
                        return;
                }
                else {
                        executeHistory(args, fd, str);
                }
        }
        //other non buildin commands
        else {
                pid_t cpid = fork();
                if (cpid < 0) {
                        perror("fail to create child process");
                }
                if (cpid == 0) { //only child goes here
                        if (pipeCount > 0) { //piped command
                                //redirect read on last command
                                if (dup2(fd[pipeCount-1][0], STDIN_FILENO) == -1) {
                                        perror("dup2 failed");
                                }

                                //close pipes
                                int j;
                                for (j = 0; j < pipeCount; j++) {
                                        close(fd[j][0]);
                                        close(fd[j][1]);
                                }
                        }

                        //execute command
                        execvp(args[0], args);
                        perror("execvp failed");
                        exit(EXIT_SUCCESS);
                }
        }
}

//exit the shell
void shellExit(char *s) {
        if (strcmp(s, "exit") == 0) {
                //free allocated memory
                int i;
                for (i = 0; i < 100; i++) {
                        free(commandLines[i]);
                }
                exit(EXIT_SUCCESS);
        }
}

//implementation of cd command
void cd(char *dir) {
        if (dir == NULL) { //cd with no argument
                perror("cd requires an argument");
                return;
        }

        int status = chdir(dir);
        if (status != 0) {
                perror("fail to change current working directory");
        }
}

//implementation of history command
void history(char *arg) {
        int i;
        if (commandLineCount < 100) { //memory not full
                for (i = 0; i < commandLineCount; i++) {
                        if (arg == NULL) { //history
                                printf("%d %s\n", i, commandLines[i]);
                        }
                        else if (strcmp(arg, "-c") == 0) { //history [-c]
                                strcpy(commandLines[i], "");
                        }
                }
        }
        else { //commandLineCount >= 100
                for (i = 0; i < 100; i++) {
                        if (arg == NULL) { //history
                                printf("%d %s\n", i, commandLines[position]);
                        }
                        else if (strcpy(arg, "-c") == 0) { //history [-c]
                                strcpy(commandLines[position], "");
                        }
                        position++;
                        if (position == 100) { //check out of bound
                                position = 0;
                        }
                }
        }
}

//execution of history command
void executeHistory(char *args[], int fd[][2], char str[]) {
        if (args[1] == NULL || strcmp(args[1], "-c") == 0) { //history or history [-c]
                storeCommandLine(str); //store in memory before execution
                history(args[1]);
                if (args[1] != NULL && commandLineCount < 100) { //history -c with not full memory
                        //restore to position 0 after clearing history
                        position -= commandLineCount;
                        commandLineCount = 0;
                }
        }
        else if (isNumber(args[1]) == 1) { //history [offset]
                int offset = atoi(args[1]);
                char s[MAX_SIZE];

                //copy command line at corresponding offset to s
                if (commandLineCount < 100) {
                        if (offset >= 0 && offset < commandLineCount) { //valid offset
                                strcpy(s, commandLines[offset]);
                        }
                        else {
                                perror("offset out of bound");
                                storeCommandLine(str);
                                return;
                        }
                }
                else { //commandLineCount >= 100
                        if (offset >= 0 && offset < 100) { //offset between 0 and 99
                                if (commandLineCount % 100 != 0) { //oldest command is not at first position
                                        strcpy(s, commandLines[(offset+position) % 100]);
                                }
                                else {
                                        strcpy(s, commandLines[offset]);
                                }
                        }
                        else {
                                perror("offset out of bound");
                                storeCommandLine(str);
                                return;
                        }
                }
                argCount = 0;
                parseLine(s, args, fd); //parse s
                storeCommandLine(str);
                strcpy(str, ""); //prevent history to be store twice in commandLines
                execute(fd, args, str);
        }
        else { //invalid argument
                perror("not a valid argument for history");
                storeCommandLine(str);
        }
}

//check if a string is numerical
int isNumber(char *s) {
        int n;
        while (*s != '\0') {
                n = isdigit(*s);
                if (n == 0) { //not a number
                        return 0;
                }
                s++;
        }
        return 1; //is a number
}
