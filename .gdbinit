#source gdb.scm
set env CK_FORK=no
set follow-exec-mode new
set print repeats 0
file /bin/s6-tcpclient
#break main
#break fsm_frontend_foreign
#break fsm_wireformat
#break starter
#break sus_select
#break sus_read
#break sus_write
#break sus_lend
#break sus_borrow
#break sus_return
