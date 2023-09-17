#include "systemcalls.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    bool ret = true;;
    if(NULL == cmd) {
        ret = false;
    }
    else if(-1 == system(cmd)) {
        ret = false;
    }

    return ret;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

static inline bool is_abs_path(char *path) {
    bool ret = true;
    if(!path || *path != '/') ret = false;
    
    return ret;
}

static inline bool is_echo_cmd(char *cmd) {
    int length = strlen(cmd);
    int i = 0;
    while(i + 4 <= length) {
        if(strncmp(&cmd[i], "echo", 4) == 0 && (i+4) == length) {
            return true;
        }
        ++i;
    }

    return false;
}

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    pid_t pid = fork();
    bool ret = true;
    int status;
    assert(pid != -1);

    if(pid == 0) {
        // i = 0;
        // while(command[i]!=NULL) {
        //     printf("%s\n", command[i++]);
        // }
        (void)execv(command[0], command);
        perror("execv");
    }
    
    if(pid != 0) {
        if(waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }

        fflush(stdout);

        printf("PID status: %d ,%d\n", status, ret);
        if(status != 0) {
            printf("status is not zero\n");
            ret = false;
        }
    }

    if(!is_abs_path(command[2])) {
        ret = false;
    }
    

    va_end(args);

    return ret;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        va_end(args);
        return false;
    }

    pid_t pid = fork();
    bool ret = true;
    int status;

    if (pid == -1) {
        perror("fork");
        va_end(args);
        return false;
    }

    if(pid == 0) {
        close(pipe_fd[0]);


        // Redirect stdout to the outputfile
        int output_fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(output_fd, STDOUT_FILENO);
        close(output_fd);

        // i = 0;
        // while(command[i]!=NULL) {
        //     printf("%s\n", command[i++]);
        // }
        (void)execv(command[0], command);
        perror("execv");
    }
    
    if(pid != 0) {
        close(pipe_fd[1]);

        if(waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }

        fflush(stdout);

        printf("PID status: %d ,%d\n", status, ret);
        if(status != 0) {
            printf("status is not zero\n");
            ret = false;
        }
    }

    // free(buf);    
    va_end(args);

    return true;
}
