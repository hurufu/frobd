%%{
    machine frob_common;
    alphtype unsigned char;

    stx = 0x02;
    etx = 0x03;
    fs = 0x1C;
    us = 0x1F;
    del = 0x7F;

    a = ascii - cntrl - del;
    h = xdigit;
    n = digit;
}%%
