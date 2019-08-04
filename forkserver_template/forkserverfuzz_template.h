#ifndef __FORKSERVER_TEMPLATE_H
#define __FORKSERVER_TEMPLATE_H

/*
    sample forkserver template for fuzzing program on linux. 
    krishs.patil@gmail.com (twitter @shsirk)
    
    how to use it - 
        1. manually patch this into program's main src file (call original main in place of __run_main)
        2. loop:
                store testcases in corpus directory
                run program.
            goto loop
        3. use export FORKSERVER_FUZZ=1 when fuzzing
        4. uset FORKSERVER_FUZZ when you don't want to use forkserver.
    note:
        1. once executed it removes testcase from corpus 
        2. crash stored in CRASH_[testcase] file name format. (use md5 for testcase names while generating)
 */
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <wait.h>

#define MAXPATH         4096
#define CORPUS_DIR      "./corpus"         /* corpus directory */
#define STREAM_OUT      "./stream.out"     /* redirect stdout/stderr to local file for asan messages */
#define TESTCASE_SRC    "./testcase"     /* testcase path passed to JSC as command line. */
#define CRASH_FMT       "./CRASH_%s"

#define APP_MAIN        my_main

/* disable this if you don't want to redirect child stdout/stderr in STREAM_OUT */
#define REDIRCT_STREAMS 1 

/* disable DIE_TIMEOUT if you don't want timeout kill! */
#define DIE_TIMEOUT     20

#ifdef DIE_TIMEOUT
/* store child information in case of killing it after timeout */
static pid_t __childtask;
/* alarm handler, kill child after timeout (see how it works under asan) */
void handle_timeout_kill(int signum)
{
    printf("    --- timeout kill on pid %d\n", __childtask);
    kill(__childtask, SIGTERM);
}
#endif

/* copy file routine */
static int copy_file(const char *to, const char *from)
{
    char buf[4096];
    int in, out;
    ssize_t nread;

    in = open(from, O_RDONLY);
    if (in < 0)
        return -1;

    out = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0)
        goto out_error;

    while (nread = read(in, buf, sizeof buf), nread > 0) {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(out, out_ptr, nread);

            if (nwritten >= 0) {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
                goto out_error;
        } while (nread > 0);
    }

    if (nread == 0) {
        if (close(out) < 0) {
            out = -1;
            goto out_error;
        }
        close(in);
        return 0;
    }

  out_error:
    close(in);
    if (out >= 0)
        close(out);
    return -1;
}

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

/* open STREAM_OUT and look for AddressSanitizer errors */
int is_sanitizer_crash()
{
    char buf[4096] = {0};
    int res = 0;
    ssize_t nread;
    int fd = open(STREAM_OUT, O_RDONLY);
    if (fd == -1)
        return 0;
    while (nread = read(fd, buf, (sizeof buf)-1), nread > 0)
    {
        if (strstr(buf, "AddressSanitizer") != NULL)
            { res = 1; break; }
        bzero(buf, 4096);
    }
    close(fd);
    return res;
}

int fork_fuzz(int argc, char**argv) 
{
    struct dirent *entry = 0;
    DIR *dp = 0;
    char src_path[MAXPATH];
    char dst_path[MAXPATH];
    int fd, status; 

    dp = opendir(CORPUS_DIR);
    if (dp == NULL) 
        return 1; 

    pid_t child; 
    while ((entry = readdir(dp))) {
        int crashed = 0;

        if (entry->d_type != DT_REG)
            continue;

        snprintf(src_path, MAXPATH, "%s/%s", CORPUS_DIR, entry->d_name);
        printf("[parent] fuzz - %s \n", src_path);
        /* copy corpus testcase to file specified as parameter to program */
        copy_file(TESTCASE_SRC, src_path);

        child = fork();
        if (child < 0)
            continue;
        
        if (child == 0) {
#ifdef REDIRCT_STREAMS
            fd = open(STREAM_OUT, O_WRONLY | O_CREAT | O_TRUNC, 0666);   
            dup2(fd, 1);
            dup2(fd, 2);
#endif  
            /* run original main forwarding command line args */
            APP_MAIN(argc, argv);
#ifdef REDIRCT_STREAMS
            close(fd);
#endif
            break;
        } else {
        
#ifdef DIE_TIMEOUT
            __childtask = child; 
            signal(SIGALRM, &handle_timeout_kill);
            alarm(DIE_TIMEOUT);
#endif
            printf("[parent] wait on child: %d\n", child);
            waitpid(child, &status, __WALL);

#ifdef DIE_TIMEOUT
            alarm(0);
#endif
            /* ok. check if we've crash? crash signal or address sanitizer? */
            if(WIFSTOPPED(status) && is_crash_signal(WSTOPSIG(status))) {
                crashed = 1;
            } else if (WIFSIGNALED(status) && is_crash_signal(WTERMSIG(status))) {
                crashed = 1;
            } else if(is_sanitizer_crash()){
                crashed = 1;
            }
            
            if (crashed) {
                printf("    --- crash reported on pid %d\n", child);
                snprintf(dst_path, MAXPATH, CRASH_FMT, entry->d_name);
                copy_file(dst_path, src_path);
            }
            //remove testcase from corpus directory
            unlink(src_path);

            bzero(src_path, MAXPATH);
            bzero(dst_path, MAXPATH);
        }
    }            

    closedir(dp);
    return 0;
}

int main(int argc, char**argv) 
{
    if(getenv("FORKSERVER_FUZZ")) {
        printf("%s running in forkserver mode\n", argv[0]);
        fork_fuzz(argc, argv);
    }else {
        printf("%s running in normal mode\n", argv[0]);
        APP_MAIN(argc, argv);
    }
}

#endif