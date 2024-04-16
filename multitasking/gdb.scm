(use-modules (gdb))

; The idea is to set a breakpoint at coro_transfer and each time it is hit –
; save relevant registers to a data structure, then define a command that should
; print backtrace for every item in the data structure. Ideally it should
; replace default bt command.
;
; Similar thing: https://gitlab.com/qemu-project/qemu/-/raw/master/scripts/qemugdb/coroutine.py
