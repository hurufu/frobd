#source gdb.scm
set env CK_FORK=no
file ./demo
break main
break starter
break sus_select
break sus_read
break sus_write
break sus_lend
break sus_borrow
break sus_return
