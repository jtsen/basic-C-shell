#ifndef _SHELL_H_
#define _SHELL_H_

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

pid_t fg_call(pid_t jobs[], char* names[], int job_index, int* job_list_size);
pid_t call (char* args[], int pip);
void parse_arguments(char* args[], char* new_args[], int pip);
void shell_prompt();
void free_copied_args(char **args, ...);
char** get_args(char ** parsed_args, size_t* sum);
void handler(int sig);


#endif