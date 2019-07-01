
#include "sym_reader.hh"

bool nm_symbol_reader::read(const char* filepath)
{
    long sym_start;
    char sym_type;
    char sym_name[SYM_NAME];

    std::ifstream infile(filepath);
    if (!infile.is_open())
        return false;

    std::string line;
    while (std::getline(infile, line)) {
        memset(sym_name, 0, SYM_NAME);
        sym_type = 0;
        
        sscanf(line.c_str(), "%lx %c %s", &sym_start, &sym_type, sym_name);
        if (!(sym_type == 'T' || sym_type == 't'))
            continue;

        std::string name(sym_name);
        syms.push_back(
            new symbol_info_t(
                sym_start,
                name
            )
        );
    }

    return !syms.empty();
}

