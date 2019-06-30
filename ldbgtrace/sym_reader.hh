#ifndef __LDBG_SYM_READER_INCLUDES__
#define __LDBG_SYM_READER_INCLUDES__

#include <string.h>
#include "ldbgtrace.hh"

//
// SYMBOL readers to be used by tracer. below is sample reader for (nm)
// tracer uses abstract implementation based symbol_reader to get list of symbols used to set tracepoints
// USER should rewrite their own implentation for other tools cases (like IDA, read-elf, or any other exporter)
//
class nm_symbol_reader : public symbol_reader
{
public:
    nm_symbol_reader(int is_pie):
        _is_pie(is_pie) { }
    ~nm_symbol_reader() {
        for(
            std::vector<symbol_info_t*>::iterator itr = syms.begin(); 
            itr != syms.end();
            itr++        
        ) {
            delete *itr;
        }
        syms.clear();
    }

    inline std::vector<symbol_info_t*>& symbols() { return syms; } 
    bool read(const char* filepath);

private:
    int _is_pie;
    std::vector<symbol_info_t*> syms; 
};

#endif