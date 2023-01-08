.PHONY: index clean graph-% run run-%

#CPPFLAGS := -DNO_LOGS_ON_STDERR
CFLAGS   := -Os -ggdb3 -Wall -Wextra -ffat-lto-objects -mtune=native -march=native
CFLAGS   += -fanalyzer -fanalyzer-checker=taint
LDFLAGS  := -flto
RL_FILES := $(wildcard *.rl)
RL_C     := $(RL_FILES:.rl=.c)
CFILES   := $(RL_C) frob.c log.c
OFILES   := $(CFILES:.c=.o)

run: run-01 run-02 run-T4
run-%: frob sample%
	./$< <$(word 2,$^) | od -tx1z
index: tags cscope.out
tags:
	ctags --kinds-C=+p -R .
cscope.out:
	cscope -bR

frob: $(OFILES)
	$(LINK.o) -o $@ $^ $(LDLIBS)
	strip --strip-unneeded $@
%.c: %.rl
	ragel -G2 -L $<
%.s: %.c
	$(CC) -S -o $@ -fverbose-asm -fno-asynchronous-unwind-tables $(CFLAGS) $<

graph-%: frob-frame.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
clean: F += $(wildcard $(RL_C) $(RL_C:.c=.s) $(OFILES) frob frob.s log.s tags cscope.out)
clean:
	$(if $(strip $F),$(RM) -- $F)
