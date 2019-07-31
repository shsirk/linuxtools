/*
    Author: krishs.patil@gmail.com (twitter @shsirk)
    
    Binary coverage tool for linux
    Record program execution using ptrace and breakpoints (int3)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <elf.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <libgen.h>

#include "ldbgtrace.hh"
#include "sym_reader.hh"
#include "trace_writer.hh"

using namespace std;

// enable record hit mode (default=disabled)
unsigned int record_hits = 0;

// enable pie mode (default=enabled)
// TODO: detect it runtime by examining ELF header? verify cost of implementation and runtime.
unsigned int is_pie = 0;

// enable verbose mode
unsigned int verbose = 0;

bool disable_break_on_first_hit = true;

char *module_to_trace = NULL;

static void fatal_error_report(const char* message)
{
  fprintf(stderr,
          "\n=======================================================================================\n"
          "[FATAL] %s\n"
          "=========================================================================================\n"
          ,message);
   ::exit(1);
}

module_range_t dbgtracer::get_module_range(pid_t pid, char* module_name)
{
    FILE *fp;
    char filename[SYM_NAME];
    char line[LINE_SIZE];

    long start=0, end=0;
    char permissions[5];
    char str[LINE_SIZE];
    
    sprintf(filename, "/proc/%d/maps", pid);
    fp = fopen(filename, "r");
    if(fp == NULL) {
        fprintf(stderr, "error while reading %s\n",  filename);
        ::exit(1);
    }

    while(fgets(line, LINE_SIZE, fp) != NULL) {
        sscanf(line, "%lx-%lx %s %s %s %s %s", &start, &end, permissions, str, str, str, str);
        if(strstr(line, module_name) && strstr(permissions, "x")) {
          if(verbose)
            fprintf(stderr, "[verbose] module range [%s] @ 0x%016lx-%016lx\n", str, start, end);
          break;
        }
        memset(line, 0, LINE_SIZE);
    }

    fclose(fp);
    return std::make_pair<address_t, address_t>(start, end);
}

void dbgtracer::set_batch_breakpoints(pid_t pid, module_range_t& module_range)
{
    //TODO: how multiple modules supported?
    address_t m_start = module_range.first, m_end = module_range.second;
    std::vector<symbol_info_t*>& syms = _sym_reader.symbols();
    for(std::vector<symbol_info_t*>::iterator itr = syms.begin(); 
        itr != syms.end(); 
        itr++) {
        address_t sym_addr = is_pie? (*itr)->offset+ m_start : (*itr)->offset; 
        if(_breaks.exists_key(sym_addr))
            continue;

        if (!(sym_addr >= m_start && sym_addr <= m_end)) {
            if (verbose)
                fprintf(stderr, "[verbose] sym (%016llx) %s is outside text range\n",
                    sym_addr, (*itr)->name.c_str());
            continue;
        }

        if (verbose) {
            fprintf(stderr, "[verbose] bp @ %016llx %s\n", 
                sym_addr, (*itr)->name.c_str());
        }

        ulong original=ptrace(PTRACE_PEEKTEXT, pid, sym_addr, NULL), 
            breaktrap=0;
        assert (original != -1);
        breaktrap = (original & ~0xff) | 0xcc;
        assert (
            ptrace(PTRACE_POKETEXT, pid, sym_addr, breaktrap) >= 0);

        _breaks.insert(sym_addr, new trace_info_t(
            original, breaktrap, *itr
        ));
    }
}

void dbgtracer::handle_break(pid_t pid)
{
    struct user_regs_struct registers;
    address_t trace_loc = 0;
    assert(
        ptrace(PTRACE_GETREGS, pid, NULL, &registers) >= 0);

    registers.rip--;
    trace_loc = registers.rip;
    assert(_breaks.exists_key(registers.rip));

    trace_info_t* tinfo = _breaks.value(registers.rip);
    assert(tinfo && tinfo->original_word() != 0);

    assert(
        ptrace(PTRACE_POKETEXT, pid, registers.rip, tinfo->original_word()) != -1);
    assert(
        ptrace(PTRACE_SETREGS, pid, NULL, &registers) >= 0);

    // let custom writer write this message.
    _trace_writer.write(tinfo);

    if (!disable_break_on_first_hit) {
        assert(
            ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL) >= 0);
        wait(NULL);      
        assert(
            ptrace(PTRACE_POKETEXT, pid, trace_loc, tinfo->breaktrap_word()) >= 0);
    }
}

void dbgtracer::handle_crash()
{
    fatal_error_report("target crashed unexpectedly, TODO: print more analysis");
}

void dbgtracer::debugloop(pid_t child)
{
    ptrace(PTRACE_SETOPTIONS, child, NULL, PTRACE_O_TRACECLONE);
    _threads.insert(child);

    // apply breakpoints 
    module_range_t m_range = get_module_range(child, basename(module_to_trace));
    set_batch_breakpoints(child, m_range);

    int ret = ptrace(PTRACE_CONT, child, NULL, NULL);

    //debug loop starts until all threads are stopped.
    while(true)
    {
        pid_t tid = waitpid(-1, &_w_status, __WALL);

        if(tid == -1)
            break;

        if(_threads.find(tid) == _threads.end())
        {
            fatal_error_report("unknown thread trapped!");
            break;
        }

        if(WIFSTOPPED(_w_status) && WSTOPSIG(_w_status) == SIGTRAP)
        {
            pid_t new_child;
            if(((_w_status >> 16) & 0xffff) == PTRACE_EVENT_CLONE)
            {
                if(ptrace(PTRACE_GETEVENTMSG, tid, 0, &new_child) != -1)
                {        
                    _threads.insert(new_child);
                    ptrace(PTRACE_CONT, new_child, 0, 0);

                    if (verbose)
                        fprintf(stderr, "[verbose] thread %d created\n", new_child);
                }

                ptrace(PTRACE_CONT, tid, 0, 0);
                continue;
            }
        }

        if(WIFEXITED(_w_status))
        {
            _threads.erase(tid);
            if (verbose)
                fprintf(stderr, "[verbose] %d exited with status %d\n", tid, WEXITSTATUS(_w_status));

            if(_threads.size() == 0)
                break;
        }
        else if(WIFSIGNALED(_w_status))
        {
            _threads.erase(tid);
            if(verbose)
                fprintf(stderr, "[verbose] child %d killed by signal %d\n", tid, WTERMSIG(_w_status));

            if(_threads.size() == 0)
                break;
        }
        else if(WIFSTOPPED(_w_status))
        {
            int code = WSTOPSIG(_w_status);
            switch(code) {
                case SIGTRAP:
                    handle_break(tid); break;
                case SIGSEGV:
                case SIGABRT:
                case SIGINT:
                case SIGILL:
                case SIGFPE:
                case SIGTERM:
                    handle_crash(); break; 
                default:
                    break;
            }
        }

        ret = ptrace(PTRACE_CONT, tid, 1, NULL);

        continue;
    }
}

bool dbgtracer::trace(char** args, char** envp)
{
    pid_t child;
    if ((child = fork()) < 0)
        return false;

    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, child, NULL, NULL) < 0)
            return false;
        execve(args[0], args, envp);
        exit(0);
    }

    wait(&_w_status);
    debugloop(child);

    return true;
}

bool dbgtracer::trace(unsigned int pid)
{
    ptrace(PTRACE_ATTACH, pid, NULL, NULL);
    wait(&_w_status);
    debugloop(pid);

    return true;
}


void help()
{
    const char *text[] = {
        "linux process tracer using ptrace apis by @shsirk\n",
        "usage:",
        "   ./ldbgtrace [-pvhcm] -- <exe_path>",
        "   -p: enable if binary is pie enabled (default is OFF)",
        "   -v: verbose mode",
        "   -h: help",
        "   -c: coverage options (hit-count|print-summary)",
        "   -m: module to cover (not implemented as of now)",
        0
    };
    for(int i =0; text[i] ; i++)
        fprintf(stderr, "%s\n", text[i]);
    exit(1);
}

int main(int argc, char **argv, char **envp)
{
    int c;
    int app_cmd_index = 0;
    opterr = 0;

    for(;app_cmd_index<argc; app_cmd_index++) {
      if (strcmp(argv[app_cmd_index], "--") == 0) {
        argv[app_cmd_index] = 0; break;
      }
    }
    if (app_cmd_index == argc) {
      help();
    }

    while ((c = getopt (app_cmd_index, argv, "pvhcm:")) != -1) {
        switch (c) {
            case 'p':
                { is_pie = 1; break; }
            case 'v':
                { verbose = true; break; }
            case 'h':
                { help(); }
            case 'c':
                { record_hits = 1; break; }
            case 'm':
                {  module_to_trace = optarg; break; }
            case '?':
                if (optopt == 'c')
                    fprintf (stderr, "option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr, 
                        "unknown option character `\\x%x'.\n",optopt);
                exit(1);
            default:
                exit(1);
        }
    }

    if (optind >= argc) {
        help();
    }

    //
    // make execve arguments
    char** args = (char**)calloc((argc-app_cmd_index), sizeof(char*));
    int index = app_cmd_index+1, i=0;
    for (; index < argc ; index++, i++) {
        args[i] = (char*)calloc(
            strlen(argv[index])+1, sizeof(char)
        ); 
        strcpy(args[i], argv[index]);
    }

    if(!module_to_trace)
      module_to_trace = args[0];

    nm_symbol_reader s_reader;
    if (!s_reader.read("nm.out")) {
        fprintf(stderr, "unable to open default nm.out symbol file\n");
    }

    plain_printer pp;

    // invoke dbgtracer 
    dbgtracer dbg(s_reader, pp);
    dbg.trace(args, envp);

    // cleanup
    for(index=0; index < i; index++)
        free(args[index]);
    free(args);

    return 0;
}
