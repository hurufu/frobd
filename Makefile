.PHONY: index clean graph-% run run-% test tcp scan coverage

if_coverage = $(if $(findstring coverage,$(MAKECMDGOALS)),$(1),)

#CPPFLAGS := -DNO_LOGS_ON_STDERR
CPPFLAGS := -D_FORTIFY_SOURCE=3
#CPPFLAGS += -DNDEBUG
OPTLEVEL ?= g
CFLAGS   := -O$(OPTLEVEL) -ggdb3 -Wall -Wextra -ffat-lto-objects -mtune=native -march=native
CFLAGS_gcc   := -fanalyzer -fanalyzer-checker=taint -fsanitize=bounds -fsanitize-undefined-trap-on-error
CFLAGS_clang := -Xanalyzer
CFLAGS += $(CFLAGS_$(CC))
CFLAGS += $(call if_coverage,--coverage)
#CFLAGS   += -fstrict-flex-arrays
# TODO: Remove those warnings only for generated files
#CFLAGS   += -Wno-implicit-fallthrough -Wno-unused-const-variable -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter
LDFLAGS  := -flto
LDFLAGS  += $(call if_coverage,--coverage)
RL_FILES := $(wildcard *.rl)
RL_C     := $(RL_FILES:.rl=.c)
RL_O     := $(RL_FILES:.rl=.o)
CFILES   := $(RL_C) frob.c log.c utils.c serialization.c
OFILES   := $(CFILES:.c=.o)
UT_T := $(wildcard *.in)
UT_C := $(UT_T:.in=.c) utils.c serialization.c log.c
UT_O := $(UT_C:.c=.o)

run: run-01 run-02 run-T4 run-S1 run-D4
run-%: frob sample%
	./$< <$(word 2,$^) | od -tx1z
index: tags cscope.out
test: ut
	./$<
check: dejagnu-frob
dejagnu-%: % | logs
	runtest --tool $<
logs:
	mkfifo $@
coverage: test | $(RL_C) $(UT_C)
	gcov -o . $|
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

graph-%: frame.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
clean: F += $(wildcard $(RL_C) $(RL_C:.c=.s) $(UT_O) $(UT_T:.in=.c) $(UT_T:.in=.s) $(OFILES) frob frob.s log.s tags cscope.out ut)
clean: F += $(wildcard *.gcda *.gcno *.gcov)
clean:
	$(if $(strip $F),$(RM) -- $F)

tcp-server: frob
	s6-tcpserver4 -v2 0.0.0.0 5002 ./$< 1000
tcp-client: frob
	s6-tcpclient -rv localhost 5002 rlwrap ./$< 1000
scan:
	scan-build $(MAKE) clean frob
