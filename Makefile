.PHONY: index clean graph-% run run-% test tcp

#CPPFLAGS := -DNO_LOGS_ON_STDERR
OPTLEVEL ?= g
CFLAGS   := -O$(OPTLEVEL) -ggdb3 -Wall -Wextra -ffat-lto-objects -mtune=native -march=native
CFLAGS   += -fanalyzer -fanalyzer-checker=taint
# TODO: Remove those warnings only for generated files
#CFLAGS   += -Wno-implicit-fallthrough -Wno-unused-const-variable -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter
LDFLAGS  := -flto
RL_FILES := $(wildcard *.rl)
RL_C     := $(RL_FILES:.rl=.c)
RL_O     := $(RL_FILES:.rl=.o)
CFILES   := $(RL_C) frob.c log.c utils.c
OFILES   := $(CFILES:.c=.o)
UT_T := $(wildcard *.in)
UT_O := $(UT_T:.in=.o) utils.o

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
ut: LDLIBS   := -lcheck -lsubunit -lm
ut: $(UT_O) $(RL_O)
	$(LINK.o) -o $@ $^ $(LDLIBS)
frob: $(OFILES)
	$(LINK.o) -o $@ $^ $(LDLIBS)
	strip --strip-unneeded $@
%.c: %.rl
	ragel -G2 -L $<
%.s: %.c
	$(CC) -S -o $@ -fverbose-asm -fno-asynchronous-unwind-tables $(CFLAGS) $<
%.c: %.in
	checkmk $< >$@

graph-%: frob-frame.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
clean: F += $(wildcard $(RL_C) $(RL_C:.c=.s) $(UT_O) $(UT_T:.in=.c) $(UT_T:.in=.s) $(OFILES) frob frob.s log.s tags cscope.out ut)
clean:
	$(if $(strip $F),$(RM) -- $F)

tcp: frob
	s6-tcpserver4 -v2 0.0.0.0 5002 ./$< 1000
