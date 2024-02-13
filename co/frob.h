#pragma once

struct args_wireformat {
    int fdin, idto;
};

struct args_frontend_foreign {
    const int idfrom, idto;
    int cs;
};

struct args_frontend_internal {
    const int fdout, idfrom;
    int cs;
};

struct args_frontend_timer {
    int cs;
};

int fsm_wireformat(const struct args_wireformat*);
int fsm_frontend_foreign(struct args_frontend_foreign*);
int fsm_frontend_internal(struct args_frontend_internal*);
int fsm_frontend_timer(struct args_frontend_timer*);
