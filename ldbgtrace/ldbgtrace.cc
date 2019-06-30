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

/** address_t current arch IP */

long dbgtracer::get_base_address(char* module_name)
{
    FILE *fp;
    char filename[SYM_NAME];
    char line[LINE_SIZE];

    long start, end;
    char permissions[5];
    char str[LINE_SIZE];
    
    sprintf(filename, "/proc/%d/maps", _pid);
    fp = fopen(filename, "r");
    if(fp == NULL) {
        fprintf(stderr, "error while reading %s\n",  filename);
        exit(1);
    }
    while(fgets(line, 85, fp) != NULL) {
        sscanf(line, "%lx-%lx %s %s %s %s %s", &start, &end, permissions, str, str, str, str);
        if(strstr(line, module_name) && strstr(permissions, "x")) {
            break;
        }
    }
    fclose(fp);
    return start;
}

void dbgtracer::set_batch_breakpoints(ulong imagebase)
{
    //TODO: how multiple modules supported?
    std::vector<symbol_info_t*>& syms = _sym_reader.symbols();
    for(std::vector<symbol_info_t*>::iterator itr = syms.begin(); 
        itr != syms.end(); 
        itr++) {
        address_t sym_addr = (*itr)->offset+imagebase; 
        if(_breaks.exists_key(sym_addr))
            continue;
        
        if (verbose)
            fprintf(stderr,
                "[verbose] bp @ %016llx %s\n", sym_addr, (*itr)->name.c_str());

        ulong original=ptrace(PTRACE_PEEKTEXT, _pid, sym_addr, NULL), 
            breaktrap=0;
        assert (original != -1);
        breaktrap = (original & ~0xff) | 0xcc;
        assert (
            ptrace(PTRACE_POKETEXT, _pid, sym_addr, breaktrap) >= 0);
            
        _breaks.insert(sym_addr, new trace_info_t(
            original, breaktrap, *itr
        ));
    }
}

void dbgtracer::handle_break()
{
    struct user_regs_struct registers;
    assert(
        ptrace(PTRACE_GETREGS, _pid, NULL, &registers) >= 0);

    registers.rip--;
    if(
        _breaks.exists_key(registers.rip)
    ) {
        trace_info_t* tinfo = _breaks.value(registers.rip);
        if(tinfo && tinfo->original_word() != 0) {
            assert(
                ptrace(PTRACE_POKETEXT, _pid, registers.rip, tinfo->original_word()) != -1);
            assert(
                ptrace(PTRACE_SETREGS, _pid, NULL, &registers) >= 0);

            // let custom writer write this message.
            _trace_writer.write(tinfo);
        }
    }
}

void dbgtracer::debugloop()
{
    while (true) {
        assert (
            ptrace(PTRACE_CONT, _pid, NULL, NULL) >= 0);

        wait(&_w_status);

        if (WIFSTOPPED(_w_status) && WSTOPSIG(_w_status) == SIGTRAP) 
            handle_break();
        else { break; } 
    }
}

bool dbgtracer::trace(char** args, char** envp)
{
    if ((_pid = fork()) < 0)
        return false;

    if (_pid == 0) {
        if (ptrace(PTRACE_TRACEME, _pid, NULL, NULL) < 0)
            return false;
        execve(args[0], args, envp);
        exit(0);
    }

    wait(&_w_status);
    
    ulong imagebase = get_base_address(basename(args[0]));
    nm_symbol_reader reader(is_pie); 
    set_batch_breakpoints(imagebase);
    
    debugloop();
    
    return true;
}

bool dbgtracer::trace(unsigned int pid)
{
    // process attach is not yet implemented.
    return false;
}


void help()
{
    const char *text[] = {
        "linux process tracer using ptrace apis by @shsirk\n",
        "usage:",
        "   ./ldbgtrace [-pvhcm] <exe_path>",
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
    opterr = 0;
    while ((c = getopt (argc, argv, "pvhcm:")) != -1) {
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
                {  } //cvalue = optarg;
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
    char** args = (char**)calloc((argc-optind+1), sizeof(char*));
    int index = optind, i=0;
    for (; index < argc ; index++, i++) {
        args[i] = (char*)calloc(
            strlen(argv[index])+1, sizeof(char)
        ); 
        strcpy(args[i], argv[index]);
    }

    nm_symbol_reader s_reader(
        is_pie
    );
    if (!s_reader.read("nm.out")) {
        fprintf(stderr, "unable to open default nm.out symbol file\n");
    }

    plain_printer pp;

    // invoke dbgtracer 
    dbgtracer dbg(s_reader, pp);
    dbg.trace(args, envp);

    // cleanup
    for(index=0; index < i; index++)
        free(args[i]);
    free(args);

    return 0;
}