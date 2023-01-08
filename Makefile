.PHONY: index clean graph-% run run-%

CFLAGS  := -Os -g0 -Wall -Wextra -ffat-lto-objects -mtune=native -march=native
LDFLAGS := -flto
RL_FILES:= $(wildcard *.rl)
RL_C    := $(RL_FILES:.rl=.c)

run: run-01 run-02 run-T4
run-%: main sample%
	./$< <$(word 2,$^)
index: tags cscope.out
tags:
	ctags --kinds-C=+p -R .
cscope.out:
	cscope -bR

main: main.c $(RL_C)
	$(LINK.c) -o $@ $^ $(LDLIBS)
	strip --strip-unneeded $@
%.c: %.rl
	ragel -G2 -L $<
%.s: %.c
	$(CC) -S -o $@ -fverbose-asm -fno-asynchronous-unwind-tables $(CFLAGS) $<

graph-%: frob-frame.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
clean: F += $(wildcard $(RL_C) $(RL_C:.c=.s) main main.s tags cscope.out)
clean:
	$(if $(strip $F),$(RM) -- $F)
