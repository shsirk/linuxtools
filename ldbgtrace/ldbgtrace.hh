#ifndef __LDBG_TRACE_INCLUDES__
#define __LDBG_TRACE_INCLUDES__

#include <stdlib.h>
#include <sys/ptrace.h>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <assert.h>
using namespace std; 

const uint LINE_SIZE = 1024;
const uint SYM_NAME  = 1024;
const uint MAP_SIZE  = 65536;
const long FAKE_BASE = 0x40000000;

typedef long long address_t;
typedef std::pair<address_t, address_t> module_range_t;

//
// SYMBOL information with name and offset relative to image load. 
struct symbol_info_t
{
    ulong offset;
    std::string name;

    symbol_info_t(ulong o, std::string& s):
        offset(o), name(s) {}
};

//
// abstract for symbol reader (concrete nm reader is implemented)
// custom implementation for other exporters supported via implementing below interface.
class symbol_reader {
public:
    virtual bool read(const char* filepath) = 0;
    virtual std::vector<symbol_info_t*>& symbols() = 0;
};

//
// set information about debuggee tracepoints 
//
class trace_info_t 
{
public:
    trace_info_t(ulong o, ulong b, symbol_info_t* sym):
        original(o), breaktrap(b), hits(0), symbol(sym) {}

    inline ulong original_word() const { return original; }
    inline ulong breaktrap_word() const { return breaktrap; }
    inline void inc_hits() { hits++; }
    inline symbol_info_t* sym() const { return symbol; }  
private:
    ulong original;
    ulong breaktrap;
    ulong hits;
    symbol_info_t* symbol;
};

//
// abstract for trace handler (invoked in breakpoint handler for each trace)
// various handlers are supported. custom needs to implement below interface.
class trace_writer {
public:
    virtual void write(trace_info_t *tinfo) = 0;
};

//
// breakpoint_address to trace_info_t mapping 
//
class trace_map 
{
public:
    trace_map() {}
    ~trace_map() {
        if(tmap.size() > 0) {
            for (std::map<address_t, trace_info_t*>::iterator itr = tmap.begin(); itr != tmap.end(); ++itr)
                delete itr->second;
            tmap.clear();
        }
    }
    inline void insert(address_t ip, trace_info_t* data) {
        tmap[ip] = data;
    }
    inline bool exists_key (address_t ip) const { return tmap.find(ip) != tmap.end(); }
    inline trace_info_t* value (address_t ip) { return tmap[ip]; }
private:
    std::map<address_t, trace_info_t*> tmap;
};

//
// debugger 
//
class dbgtracer 
{
public:
    dbgtracer(symbol_reader& sym_r, trace_writer& trace_w):
        _sym_reader(sym_r), _trace_writer(trace_w)
        {}
    // execute new process and begin tracing
    bool trace(char** args, char** envp);
    // attach to the existing process and begin tracing
    bool trace(unsigned int pid);

private:
    module_range_t get_module_range(char* module_name);
    void set_batch_breakpoints(module_range_t& module_range);
    void handle_break();
    void debugloop();
private:
    pid_t _pid;
    int _w_status;
    trace_map _breaks;

    symbol_reader& _sym_reader;
    trace_writer& _trace_writer;
};

#endif
