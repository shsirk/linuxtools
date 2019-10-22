/*
    sample fuzz harness for linux using fork and execve
    krishs.patil@gmail.com 

    redirect stdout/stderr of child to $STREAM_OUT_PATH (in case of ASAN messages)
    and also check for child process exit codes to determine the if it crashed 
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#define STREAM_OUT_PATH "./child_err_out_redir"

int is_crash_signal(int code)
{
    switch(code) {
        case SIGTRAP:
        case SIGSEGV:
        case SIGABRT:
        case SIGINT:
        case SIGILL:
        case SIGFPE:
        // NOTE: 
        // SIGTERM? how about killing of timeout process externally?
        // kilall all generates SIGTERM (may result false alarm here)
        // Externally or internally killing child we can use SIGKILL (kill -9 pid_of_child)
        //case SIGTERM:  
            return 1;
        default:
            return 0;
    }
}

int main(int argc, const char * argv[], char* env[]) 
{
    pid_t child;
    int fd;
    int status;
    char buf[1024] = {0};

    if (argc == 1) 
    {
        printf("%s <full_path_to_bin> [args_to_bin]\n", argv[0]);
        return 1; 
    }

    char **args = (char**)argv;
    args++; 

    child = fork();
    if(child == 0)
    {
        fd = open(STREAM_OUT_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);   
        dup2(fd, 1);
        dup2(fd, 2);                   
        execve(args[0], args, env);                   
        close(fd);                                          
    }

    waitpid(child, &status, __WALL);

    if(WIFSTOPPED(status) && is_crash_signal(WSTOPSIG(status)))
    {
        snprintf(
            buf, 1024, "[CRASH!] child stopped with signal code %08d\n",
            WSTOPSIG(status)
        );

    } else if (WIFSIGNALED(status) && is_crash_signal(WTERMSIG(status))) {
        snprintf(
            buf, 1024, "[CRASH!] child signaled with signal code %08d\n",
            WTERMSIG(status)
        );
    }

    int buflen = strlen(buf);
    if (buflen > 0) {
        fd = open(STREAM_OUT_PATH, O_WRONLY | O_CREAT, 0666);
        write(fd, buf, buflen);
        close(fd);
    } 
    
    return 0;
}
