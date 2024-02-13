
#line 1 "frontend.rl"
#include "comultitask.h"
#include "frob.h"
#include <stdbool.h>
#include <sys/timerfd.h>


#line 31 "frontend.rl"



#line 14 "frontend.c"
static const int frontend_start = 5;
static const int frontend_first_final = 5;
static const int frontend_error = 0;

static const int frontend_en_main = 5;


#line 34 "frontend.rl"

static bool is_idempotent(const char* const msg) {
    switch (msg[5]) {
        case 'T':
        case 'P':
            return true;
    }
    return false;
}

static int fsm_exec(int* cs, const char* p, const char* const pe) {
    char acknak;
    int fdout = 5;
    
#line 37 "frontend.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( ( *cs) )
	{
tr5:
#line 23 "frontend.rl"
	{ acknak = 0x06; }
#line 25 "frontend.rl"
	{ sus_write(fdout, &acknak, 1); }
	goto st5;
tr7:
#line 24 "frontend.rl"
	{ acknak = 0x15; }
#line 25 "frontend.rl"
	{ sus_write(fdout, &acknak, 1); }
	goto st5;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
#line 59 "frontend.c"
	switch( (*p) ) {
		case 2u: goto tr5;
		case 10u: goto st1;
		case 21u: goto tr7;
	}
	goto st0;
st0:
( *cs) = 0;
	goto _out;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	switch( (*p) ) {
		case 0u: goto st2;
		case 6u: goto st5;
	}
	goto st0;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
	switch( (*p) ) {
		case 0u: goto st3;
		case 6u: goto st5;
	}
	goto st0;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
	switch( (*p) ) {
		case 0u: goto st4;
		case 6u: goto st5;
	}
	goto st0;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
	if ( (*p) == 6u )
		goto st5;
	goto st0;
	}
	_test_eof5: ( *cs) = 5; goto _test_eof; 
	_test_eof1: ( *cs) = 1; goto _test_eof; 
	_test_eof2: ( *cs) = 2; goto _test_eof; 
	_test_eof3: ( *cs) = 3; goto _test_eof; 
	_test_eof4: ( *cs) = 4; goto _test_eof; 

	_test_eof: {}
	_out: {}
	}

#line 48 "frontend.rl"
    return -1;
}

void fsm_frontend_init(int* const cs) {
    
#line 120 "frontend.c"
	{
	( *cs) = frontend_start;
	}

#line 53 "frontend.rl"
}

int fsm_frontend_foreign(struct args_frontend_foreign* const a) {
    char acknak = 0x00;
    ssize_t bytes;
    const char* p;
    while ((bytes = sus_lend(a->idfrom, &p, 1)) > 0) {
        const char* const pe = p + 1;
        fsm_exec(&a->cs, p, pe);
    }
    return -1;
}

int fsm_frontend_internal(struct args_frontend_internal* const a) {
    ssize_t bytes;
    const char* msg;
    while ((bytes = sus_lend(a->idfrom, &msg, 0)) > 0) {
        const char* p = (char[]){is_idempotent(msg) ? 0x0A : 0x0D}, * const pe = p + 1;
        fsm_exec(&a->cs, p, pe);
    }
    return -1;
}

int fsm_frontend_timer(struct args_frontend_timer* const a) {
    const int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    ssize_t bytes;
    unsigned char buf[8];
    while ((bytes = sus_read(fd, buf, sizeof buf)) > 0) {
        const char* p = (char[]){0}, * const pe = p + 1;
        fsm_exec(&a->cs, p, pe);
    }
    return -1;
}
