#include "frob.h"
#include "utils.h"

%%{
    machine frob_attrs;
    include frob_common "common.rl";

    action C {
        c = fpc;
    }
    action Unit {
        if (i < elementsof(*out))
            COPY((*out)[i++], c, fpc);
    }

    s = a* >C us;

    main := s+ @Unit fs;

    write data;
}%%

int frob_extract_additional_attributes(const byte_t** pp, const byte_t* pe, char (* const out)[10][16]) {
    int cs;
    size_t i = 0;
    const byte_t* c, * p = *pp;
    %%{
        write init;
        write exec;
    }%%
    *pp = p;
    return cs < %%{ write first_final; }%%;
}
