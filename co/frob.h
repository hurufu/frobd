#pragma once

struct args_frontend_foreign {
    int cs;
};

struct args_frontend_internal {
    int cs;
};

struct args_frontend_timer {
    int cs;
};

int fsm_wireformat(int, int);
int fsm_frontend_foreign(struct args_frontend_foreign*);
int fsm_frontend_internal(struct args_frontend_internal*);
int fsm_frontend_timer(struct args_frontend_timer*);
