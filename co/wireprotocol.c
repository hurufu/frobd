
#line 1 "wireprotocol.rl"
#include "comultitask.h"
#include "frob.h"
#include <unistd.h>


#line 21 "wireprotocol.rl"



#line 13 "wireprotocol.c"
static const int wireformat_start = 6;
static const int wireformat_first_final = 6;
static const int wireformat_error = 0;

static const int wireformat_en_main = 6;


#line 24 "wireprotocol.rl"

int fsm_wireformat(const struct args_wireformat* const a) {
    char* start = NULL, * end = NULL;
    int fd = 1;
    unsigned lrc;
    ssize_t bytes;
    char buf[1024];
    int cs;
    int to = 4;
    char* p = buf, * pe = p;
    
#line 33 "wireprotocol.c"
	{
	cs = wireformat_start;
	}

#line 35 "wireprotocol.rl"
    while ((bytes = sus_read(fd, buf, sizeof buf)) > 0) {
        
#line 41 "wireprotocol.c"
	{
	if ( p == pe )
		goto _test_eof;
	switch ( cs )
	{
st6:
	if ( ++p == pe )
		goto _test_eof6;
case 6:
	if ( (*p) == 2u )
		goto tr1;
	goto st1;
st1:
	if ( ++p == pe )
		goto _test_eof1;
case 1:
	if ( (*p) == 2u )
		goto tr1;
	goto st1;
tr1:
#line 10 "wireprotocol.rl"
	{ lrc = 0; }
	goto st2;
st2:
	if ( ++p == pe )
		goto _test_eof2;
case 2:
#line 69 "wireprotocol.c"
	if ( (*p) == 3u )
		goto st0;
	goto tr2;
tr2:
#line 16 "wireprotocol.rl"
	{ start = p; }
#line 11 "wireprotocol.rl"
	{ lrc ^= (*p); }
	goto st3;
tr4:
#line 11 "wireprotocol.rl"
	{ lrc ^= (*p); }
	goto st3;
st3:
	if ( ++p == pe )
		goto _test_eof3;
case 3:
#line 87 "wireprotocol.c"
	if ( (*p) == 3u )
		goto tr5;
	goto tr4;
tr5:
#line 11 "wireprotocol.rl"
	{ lrc ^= (*p); }
	goto st4;
st4:
	if ( ++p == pe )
		goto _test_eof4;
case 4:
#line 99 "wireprotocol.c"
	goto tr6;
tr6:
#line 12 "wireprotocol.rl"
	{
        if (lrc != (*p))
            {p++; cs = 5; goto _out;}
    }
#line 17 "wireprotocol.rl"
	{ sus_borrow(to, NULL); }
	goto st5;
st5:
	if ( ++p == pe )
		goto _test_eof5;
case 5:
#line 114 "wireprotocol.c"
	goto st6;
st0:
cs = 0;
	goto _out;
	}
	_test_eof6: cs = 6; goto _test_eof; 
	_test_eof1: cs = 1; goto _test_eof; 
	_test_eof2: cs = 2; goto _test_eof; 
	_test_eof3: cs = 3; goto _test_eof; 
	_test_eof4: cs = 4; goto _test_eof; 
	_test_eof5: cs = 5; goto _test_eof; 

	_test_eof: {}
	_out: {}
	}

#line 37 "wireprotocol.rl"
    }
}
