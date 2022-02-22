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
#include "shell.h"

#define MAX_COMMAND_LENGTH (1001)
#define MAX_DIRECTORY_LENGTH (1001)
#define MAX_ARGS (500)
#define MAX_SUSPENDED_JOBS (100)

pid_t fg_call(pid_t jobs[], char* names[], int job_index, int* job_list_size){
    //https://denniskubes.com/2012/08/20/is-c-pass-by-value-or-reference/
    pid_t pid = jobs[job_index-1];
    char *pid_name = names[job_index-1];
    for(int i = job_index-1; i<*job_list_size; ++i){
        jobs[i] = jobs[i+1];
        names[i] = names[i+1];
    }
    (*job_list_size)--;

    int status;
    if(kill(pid, SIGCONT) == 0){
        waitpid(pid, &status, WUNTRACED);
    }
    else{
        fprintf(stderr, "SIGCONT kill command failed.");
    }

    if(WIFSTOPPED(status)){
        names[*job_list_size] = pid_name;
        return pid;
    }
    else if (WIFEXITED(status)){
        kill(pid, SIGKILL);
        free(pid_name);
        return 0;
    }
    else if(WIFSIGNALED(status)){
        free(pid_name);
        return 0;
    }
    else if(WTERMSIG(status)){
        free(pid_name);
        return 0;
    }
    else{
        free(pid_name);
        return 0;
    }
}


pid_t call (char* args[], int pip){
    char * new_args[MAX_ARGS] = {NULL};
    new_args[0] = args[0];
    if(pip){
        parse_arguments(args, new_args, pip);
        execvp(new_args[0], new_args);
        printf("Error: invalid program\n");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1){
        perror("fork error");
        //https://stackoverflow.com/questions/13667364/exit-failure-vs-exit1
        exit(EXIT_FAILURE);
    }

    int status;

    if(!pid){
        //https://stackoverflow.com/questions/28493392/executing-redirection-and-in-custom-shell-in-c
        parse_arguments(args, new_args, pip);
        execvp(new_args[0], new_args);
        printf("Error: invalid program\n");
        exit(EXIT_FAILURE);
    }
    else{
        //https://stackoverflow.com/questions/21248840/example-of-waitpid-in-use
        waitpid(pid, &status, WUNTRACED);
        if(WIFSIGNALED(status)){
            return 0;
        }
        else if(WIFSTOPPED(status)){
            //https://unix.stackexchange.com/questions/404054/how-is-a-process-group-id-set
            return pid;
        }
        else if(WEXITSTATUS(status)){
            return 0;
        }
        else if(WIFEXITED(status)){
            return 0;
        }
    }
    return 0;
}

void parse_arguments(char* args[], char* new_args[], int pip){
    int in; int out; int in_true=0; int out_true=0;
    for(int i = 0; i<MAX_ARGS; ++i){
        if (!args[i]){
            break;
        }
        if(strcmp(args[i], "<<")==0){
            fprintf(stderr, "Error: invalid command\n");
            exit(EXIT_FAILURE);
        }
        if(strcmp(args[i], "<")==0){
            if((!args[i+1]) | in_true | pip){
                fprintf(stderr, "Error: invalid command\n");
                exit(EXIT_FAILURE);
            }
            in = open(args[i+1], O_RDONLY);
            if (in<0){
                fprintf(stderr, "Error: Invalid file\n");
                exit(EXIT_FAILURE);
            }
            dup2(in, STDIN_FILENO);
            close(in);
            in_true++;
            i++;
        }
        else if(strcmp(args[i], ">")==0){
            if((!args[i+1]) | out_true){
                fprintf(stderr, "Error: invalid command\n");
                exit(EXIT_FAILURE);
            }
            //https://stackoverflow.com/questions/18415904/what-does-mode-t-0644-mean/18415935
            out = creat(args[i+1], 0644);
            if(out<0){
                fprintf(stderr, "Error: Invalid file\n");
                exit(EXIT_FAILURE);
            }
            dup2(out, STDOUT_FILENO);
            close(out);
            out_true++;
            i++;
        }
        else if(strcmp(args[i], ">>")==0){
            if((!args[i+1]) | out_true){
                fprintf(stderr, "Error: invalid command\n");
                exit(EXIT_FAILURE);
            }
            out = open(args[i+1], O_WRONLY|O_CREAT|O_APPEND, 0644);
            dup2(out, STDOUT_FILENO);
            close(out);
            out_true++;
            i++;
        }
        else if(strcmp(args[i], "|")==0){
            //https://stackoverflow.com/questions/13636252/c-minishell-adding-pipelines
            if((!args[i+1]) | (!args[i-1]) | out_true){
                fprintf(stderr, "Error: invalid command\n");
                exit(EXIT_FAILURE);
            }
            char ** args_after_pipe = &args[i+1];
            int pipefd[2];
            pid_t cpid;
            if(pipe(pipefd) == -1){
                perror("pipe error");
                exit(EXIT_FAILURE);
            }
            cpid = fork();
            if(cpid == -1){
                perror("fork error");
                exit(EXIT_FAILURE);
            }
            if(!cpid){
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
                call(args_after_pipe, 1);
            }
            else{
                dup2(pipefd[1],STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);

                break;
            }

        }
        else{
            if((out_true&&in_true) | out_true | (in_true && strcmp(args[i],">") | strcmp(args[i],"|") | strcmp(args[i],">>"))){
                fprintf(stderr, "Error: invalid command\n");
                exit(EXIT_FAILURE);
            }
            new_args[i] = args[i];
        }
    }
}

void shell_prompt(){
    //https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
    char cwd[MAX_DIRECTORY_LENGTH];
    getcwd(cwd, MAX_DIRECTORY_LENGTH);
    fprintf(stdout,"[nyush %s]$ ", basename(cwd));
}

void free_copied_args(char **args, ...){

    va_list params;
    va_start(params, args);

    while(args != NULL){ //while we are not at the end of the list of arguments
        for (char **p = args; *p; ++p) { // iterate through the strings (char *) within given string array
            free(*p); // free the strings
        }
        // free(args); // don't need this line as the array is allocated on the stack
        char ** temp = va_arg(params, char **); //temporary pointer that points to the next argument
        args = temp; //point to the next argument
    }

    va_end(params); // function ends with no more arguments
}

char** get_args(char ** parsed_args, size_t * sum){
    char arguments[MAX_COMMAND_LENGTH];
    char* tokens;
    if (fgets(arguments, MAX_COMMAND_LENGTH, stdin)){

        *sum = strlen(arguments);

        //https://stackoverflow.com/questions/4513316/split-string-in-c-every-white-space
        tokens = strtok(arguments, " \n\r\t");

        //if the first token is NULL (no arguments)
        if(!tokens){
            parsed_args[0][0]='\0';
            return parsed_args;
        }


        int counter = 0;
        while (tokens != NULL){
            // printf("token: %s\n", tokens);
            char * curr = malloc(strlen(tokens)+1);
            strcpy(curr, tokens);
            for(char* p = curr; *p; ++p){
                *p = tolower((unsigned char)*p);
            }
            parsed_args[counter] = curr;
            tokens = strtok(NULL, " \n\r\t");
            ++counter;
        }
        parsed_args[counter]=NULL;
        return parsed_args;

    }
    else{
        return (char**) NULL;
    }
}

void handler(int sig){printf("\n");}

int main(){
    char* parsed_args[MAX_ARGS];
    pid_t suspended_jobs[MAX_SUSPENDED_JOBS] = {0};
    char* suspended_jobs_names[MAX_ARGS] = {NULL};
    int suspended_jobs_counter = 0;
    pid_t call_res;
    size_t sum;

    signal(SIGINT,handler);
    signal(SIGQUIT,handler);
    signal(SIGTSTP,handler);
    signal(SIGTERM,handler);
    while(1){

        shell_prompt();
        sum = 0;
        if(get_args(parsed_args, &sum)){
            if(parsed_args[0][0]=='\0'){
                continue;
            }

            if (strcmp(parsed_args[0], "cd")==0){
                if(parsed_args[1]&&!parsed_args[2]){
                    //https://stackoverflow.com/questions/1293660/is-there-any-way-to-change-directory-using-c-language
                    int chres = chdir(parsed_args[1]);
                    if (chres == -1){
                        printf("Error: invalid directory\n");
                        continue;
                    }
                    continue;
                }
                else{
                    fprintf(stderr, "Error: invalid command\n");
                    continue;
                }
            }
            else if(strcmp(parsed_args[0], "exit")==0){
                if (parsed_args[1]!=NULL){
                    fprintf(stderr, "Error: invalid command\n");
                    continue;
                }
                if(suspended_jobs_counter){
                    fprintf(stderr, "Error: there are suspended jobs\n");
                    continue;
                }
                break;
            }
            else if(strcmp(parsed_args[0],"jobs")==0){

                for(int i = 0; i<suspended_jobs_counter; ++i){
                    printf("[%d] pid: %d command: %s\n", i+1, suspended_jobs[i], suspended_jobs_names[i]);
                }
                continue;

            }
            else if(strcmp(parsed_args[0], "fg")==0){
                if(parsed_args[0] && parsed_args[1] && !parsed_args[2]){
                    int index = atoi(parsed_args[1]);
                    if (index > suspended_jobs_counter){
                        fprintf(stderr, "Error: invalid job\n");
                        continue;
                    }
                    call_res = fg_call(suspended_jobs, suspended_jobs_names, index, &suspended_jobs_counter);
                    if(call_res){
                        suspended_jobs[suspended_jobs_counter] = call_res;
                        suspended_jobs_counter++;
                    }
                    continue;
                }
                else{
                    //https://stackoverflow.com/questions/39002052/how-i-can-print-to-stderr-in-c
                    fprintf(stderr, "Error: invalid command\n");
                    continue;
                }
            }
            else{
                call_res = call(parsed_args, 0);
                if(call_res){
                    char * temp = malloc(sizeof(char)*sum);
                    for(char **p = parsed_args; *p; ++p){
                        strcat(temp, *p);
                        strcat(temp, " ");
                    }
                    suspended_jobs[suspended_jobs_counter] = call_res;
                    suspended_jobs_names[suspended_jobs_counter] = temp;
                    suspended_jobs_counter++;
                }
            }
            free_copied_args(parsed_args, NULL);
        }
        else{
            // free_copied_args(parsed_args, NULL);
            break;
        }
    }
    printf("\n");
    free_copied_args(parsed_args, suspended_jobs_names, NULL);
    return 0;
}
