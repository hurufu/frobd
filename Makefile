.PHONY: index clean graph-% run run-% test

#CPPFLAGS := -DNO_LOGS_ON_STDERR
CFLAGS   := -O0 -ggdb3 -Wall -Wextra -ffat-lto-objects -mtune=native -march=native
CFLAGS   += -fanalyzer -fanalyzer-checker=taint
LDFLAGS  := -flto
RL_FILES := $(wildcard *.rl)
RL_C     := $(RL_FILES:.rl=.c)
RL_O     := $(RL_FILES:.rl=.o)
CFILES   := $(RL_C) frob.c log.c
OFILES   := $(CFILES:.c=.o)
UT_T := $(wildcard *.t)
UT_O := $(UT_T:.t=.o)

run: run-01 run-02 run-T4 run-S1 run-D4
run-%: frob sample%
	./$< <$(word 2,$^) | od -tx1z
index: tags cscope.out
test: ut
	./$<
tags:
	ctags --kinds-C=+p -R .
cscope.out:
	cscope -bR
ut: CPPFLAGS := -DNO_LOGS_ON_STDERR
ut: LDLIBS   := -lcheck
ut: CFLAGS   += -Og -ggdb3
ut: $(UT_O) $(RL_O)
	$(LINK.o) -o $@ $^ $(LDLIBS)
frob: $(OFILES)
	$(LINK.o) -o $@ $^ $(LDLIBS)
	#strip --strip-unneeded $@
%.c: %.rl
	ragel -G2 -L $<
%.s: %.c
	$(CC) -S -o $@ -fverbose-asm -fno-asynchronous-unwind-tables $(CFLAGS) $<
%.c: %.t
	checkmk $< >$@

graph-%: frob-frame.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
clean: F += $(wildcard $(RL_C) $(RL_C:.c=.s) $(UT_O) $(UT_T:.t=.c) $(UT_T:.t=.s) $(OFILES) frob frob.s log.s tags cscope.out ut)
clean:
	$(if $(strip $F),$(RM) -- $F)
