#include "frob.h"
#include <errno.h>

%%{
    machine frob_protocol;

    alphtype unsigned int;

    variable cs (*cs);

    # TODO: Re-use type values from frob.h
    T1 =  1000;
    T2 =  1002;
    T3 =  1003;
    T4 =  1004;
    T5 =  1005;
    D0 =  2000;
    D0ok= 20000;
    D0no= 20001;
    D1 =  2001;
    D2 =  2002;
    D3 =  2003;
    D4 =  2004;
    D5 =  2005;
    D6 =  2006;
    D7 =  2007;
    D8 =  2008;
    D90=  20090;
    D9x=  20091;
    DA =  2010;
    S1 =  3001;
    S2 =  3002;
    I1 =  5001;
    P1 =  4001;
    A1 =  6001;
    A2 =  6002;
    A2ok= 60020;
    A2no= 60021;
    K0 =  7000;
    K0ok= 70000;
    K0no= 70001;
    K1 =  7001;
    K2 =  7002;
    K3 =  7003;
    K4 =  7004;
    K5 =  7005;
    K6 =  7006;
    K7 =  7007;
    K8 =  7008;
    K9 =  7009;
    M1 =  8001;
    L1 =  9001;
    B1 = 10001;
    B2ok=100020;
    B2no=100021;
    B3 = 10003;
    B4 = 10004;

    communication_test = T1 T2;
    version_negotiation = T3 T4 T5;
    receipt_printing = D2 D0ok (D6 D0)* D3 D0 | D2 D0no;
    receipt_logo_mgmt = D7 D8 | DA D0 | (D9x D0)* D90 D0;
    configuration = D4 D5;
    custom_flow = K1 K0ok ((K3|K4|K5|K6|K7|K8|K9) K0)* K2 K0 | K1 K0no;
    key_exchange = B1 B2ok B3 B4 | B1 B2no;
    transaction = S1 (P1 | I1)* S2;

    async := (L1 | configuration | version_negotiation | communication_test)*;
    encdec := key_exchange*;
    aux := (A1 A2no | A1 A2ok custom_flow)*;
    printer := (receipt_printing | receipt_logo_mgmt)*;
    main := (A1 A2 | transaction | M1)*;

    write data;
}%%

int frob_protocol_transition(int* const cs, const enum FrobMessageType t) {
    const unsigned short x[2] = { t };
    const unsigned short* p = x, * const pe = x + 1;
    if (*cs < 0)
        %% write init;

    %% write exec;

    if (false)
        return EPROTO;
    return 0;
}
