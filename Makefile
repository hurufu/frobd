.PHONY: index clean graph-% test tcp scan coverage all test-unit test-functional test-random test-mutation mutated

if_coverage = $(if $(findstring coverage,$(MAKECMDGOALS)),$(1),)

CC := gcc

#CPPFLAGS += -DNDEBUG
# Disable all logs
#CPPFLAGS := -DNO_LOGS_ON_STDERR
OPTLEVEL ?= g
# Some gcc builds (depends on the distro) set _FORTIFY_SOURCE by default,
# so we need undefine it and then redefine it
CPPFLAGS_gcc := -U_FORTIFY_SOURCE -D_GLIBCXX_ASSERTIONS
CFLAGS_gcc   := -fanalyzer -fanalyzer-checker=taint -fsanitize=bounds -fsanitize-undefined-trap-on-error -ffat-lto-objects
CFLAGS_gcc   += -Wformat -Werror=format-security -grecord-gcc-switches
CFLAGS   += -fstack-protector-all
#CFLAGS_gcc   += -fstack-clash-protection
# Those flags don't work together with normal compilation, that's why they are disabled by default
#CFLAGS_clang := --analyze -Xanalyzer
CPPFLAGS += $(CPPFLAGS_$(CC)) -D_FORTIFY_SOURCE=3
CFLAGS   := -std=gnu11 -O$(OPTLEVEL) -ggdb3 -Wall -Wextra -mtune=native -march=native -flto
CFLAGS   += $(CFLAGS_$(CC))
CFLAGS   += $(call if_coverage,--coverage)
# TODO: Remove those warnings only for generated files
CFLAGS   += -Wno-implicit-fallthrough -Wno-unused-const-variable -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter
#CFLAGS   += -fstrict-flex-arrays
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
MUTATED_O := utils.o serialization.o
NORMAL_O  := $(RL_O) $(UT_T:.in=.c) log.o

all: frob
index: tags cscope.out
test: test-unit test-functional test-random
coverage: test | $(CFILES) $(UT_C)
	gcov -o . $|
tags:
	ctags --kinds-C=+p -R .

test-random: frob
	pv -Ss100M /dev/urandom | ./$< 1 2>/dev/null 1>/dev/null
test-unit: ut
	./$<
test-functional: frob.log frob.sum
# mull-runner leaves running mutant processes 🤦
test-mutation: mut
	trap 'pkill mut' EXIT; mull-runner-15 --ld-search-path=/usr/lib ./$<
%.log %.sum: %
	runtest --tool $<
cscope.out:
	cscope -bR
ut: LDLIBS   := -lcheck -lsubunit -lm
ut: $(UT_O) $(RL_O)
	$(LINK.o) -o $@ $^ $(LDLIBS)
mut: LDLIBS := -lcheck -lsubunit -lm
mut: CC     := clang
mut: $(NORMAL_O) mutated
	$(LINK.o) -o $@ $(NORMAL_O) $(MUTATED_O) $(LDLIBS)
mutated: CFLAGS += -fexperimental-new-pass-manager -fpass-plugin=/usr/local/lib/mull-ir-frontend-15 -grecord-command-line
mutated: $(MUTATED_O)
frob: $(OFILES)
	$(LINK.o) -o $@ $^ $(LDLIBS)
	objcopy --only-keep-debug $@ $@.debug
	strip --strip-unneeded $@
	objcopy --add-gnu-debuglink=$@.debug $@
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
clean: F += $(wildcard frob.log frob.sum frob.debug mut)
clean:
	$(if $(strip $F),$(RM) -- $F)

tcp-server: frob
	s6-tcpserver4 -v2 0.0.0.0 5002 ./$< 1000 payment
tcp-client: frob | payment
	s6-tcpclient -rv localhost 5002 rlwrap ./$< 1000 $|
scan:
	scan-build $(MAKE) clean frob
