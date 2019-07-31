/*
    Author: krishs.patil@gmail.com (twitter @shsirk)
    
    Binary coverage tool for linux
    Record program execution using ptrace and breakpoints (int3)
 */

#ifndef __LDBG_TRACE_WRITER_INCLUDES__
#define __LDBG_TRACE_WRITER_INCLUDES__

#include <string.h>
#include "ldbgtrace.hh"

class plain_printer: public trace_writer
{
public:
    inline void write(trace_info_t *tinfo) {
        symbol_info_t* psym = tinfo->sym();
        fprintf(stderr, 
                "[trace] (%08lx) %s\n",
                psym->offset, psym->name.c_str());
    }
};

#endif