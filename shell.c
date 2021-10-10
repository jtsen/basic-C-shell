#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_COMMAND_LENGTH (1001)
#define MAX_DIRECTORY_LENGTH (1001)
#define MAX_ARGS (500)
#define MAX_SUSPENDED_JOBS (100)

pid_t call (char* args[]){
    pid_t pid = fork();
    if (pid == -1){
        perror("fork error");
        //https://stackoverflow.com/questions/13667364/exit-failure-vs-exit1
        exit(EXIT_FAILURE);
    }

    int status;

    if(!pid){
        execvp(args[0], &args[0]);
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

void shell_prompt(){
    //https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
    char cwd[MAX_DIRECTORY_LENGTH];
    getcwd(cwd, MAX_DIRECTORY_LENGTH);
    printf("[nyush %s]$ ", basename(cwd));
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

char** get_args(char ** parsed_args){
    char arguments[MAX_COMMAND_LENGTH];
    char* tokens;
    fgets(arguments, MAX_COMMAND_LENGTH, stdin);
            
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

void handler(int sig){printf("\n");}

int main(int argc, const char *const *argv){
    char arguments[MAX_COMMAND_LENGTH];
    char* tokens;
    char* parsed_args[MAX_ARGS];
    pid_t suspended_jobs[MAX_SUSPENDED_JOBS] = {0};
    char* suspended_jobs_names[MAX_ARGS] = {NULL};
    int suspended_jobs_counter = 0;
    signal(SIGINT,handler);
    signal(SIGQUIT,handler);
    signal(SIGTSTP,handler);

    // signal(SIGTERM,handler);
    while(1){

        shell_prompt();
        get_args(parsed_args);

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
            break;
        }
        else if(strcmp(parsed_args[0],"jobs")==0){

            for(int i = 0; i<suspended_jobs_counter; ++i){
                printf("[%d] pid: %ld command: %s\n", i+1, suspended_jobs[i], suspended_jobs_names[i]);
            }
            continue;
            
        }
        else if(strcmp(parsed_args[0], "fg")==0){
            if (parsed_args[0] && parsed_args[1] && !parsed_args[2]){

            }
            else if(parsed_args[0] && !parsed_args[1]){

            }
            else{
                //https://stackoverflow.com/questions/39002052/how-i-can-print-to-stderr-in-c
                fprintf(stderr, "Error: invalid command\n");
            }
        }
        else{
            pid_t call_res = call(parsed_args);
            if(call_res){
                char * temp = malloc(sizeof(char)*strlen(parsed_args[0]));
                strcpy(temp, parsed_args[0]);
                suspended_jobs[suspended_jobs_counter] = call_res;
                suspended_jobs_names[suspended_jobs_counter] = temp;
                suspended_jobs_counter++;
            }
        }

        free_copied_args(parsed_args, NULL);
    
    }
    free_copied_args(parsed_args, suspended_jobs_names, NULL);
    return 0;
}