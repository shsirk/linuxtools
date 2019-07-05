

Linux debugger based process tracer.

    - uses ptrace api for debugging native C/C++ application
    - use nm to extract symbols from exe (ldbgtrace looks for nm.out in current directory for breakpoints points)
    - in case stripped binary use disassembler (IDA) to extract break points from binary and write it in the form of nm output)
    

TODO:

    1. 32/64 bit testing
    2. still not robust
    3. support different symbol readers
    4. support different coverage analyzers
    5. shared module coverage? (currently only support statically compiled)
    6. more signals handling
    7. timeout support
    8. multi-threaded apps crashes on thread creation
        (Notes;
            https://blog.codef00.com/2008/01/29/linuxs-ptrace-api-sucks
            https://stackoverflow.com/questions/18577956/how-to-use-ptrace-to-get-a-consistent-view-of-multiple-threads
            https://github.com/eteran/edb-debugger/issues/77
         )
         
