.PHONY: index clean graph-% test tcp scan coverage all test-unit test-functional test-random test-mutation mutated clang-analyze

OPTLEVEL := g

# Compiler configuration #######################################################
if_coverage = $(if $(findstring coverage,$(MAKECMDGOALS)),$(1),)

CPPFLAGS_clang := -D_FORTIFY_SOURCE=3
# Some gcc builds (depends on the distro) set _FORTIFY_SOURCE by default,
# so we need undefine it and then redefine it
CPPFLAGS_gcc   := -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS
CPPFLAGS       ?= $(CPPFLAGS_$(CC))
#CPPFLAGS       += -DNDEBUG
# Disable all logs
#CPPFLAGS       += -DNO_LOGS_ON_STDERR

CFLAGS_gcc := -fanalyzer -fanalyzer-checker=taint -fsanitize=bounds -fsanitize-undefined-trap-on-error -ffat-lto-objects
CFLAGS_gcc += -Wformat -Werror=format-security -grecord-gcc-switches
CFLAGS_gcc += -fstack-protector-all
CFLAGS_gcc += -fstack-clash-protection
CFLAGS     ?= -std=gnu11 -O$(OPTLEVEL) -ggdb3 -Wall -Wextra -mtune=native -march=native -flto $(CFLAGS_$(CC))
CFLAGS     += $(call if_coverage,--coverage)
#CFLAGS   += -fstrict-flex-arrays
# TODO: Remove those warnings only for generated files
CFLAGS     += -Wno-implicit-fallthrough -Wno-unused-const-variable -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter

LDFLAGS ?= -flto
LDFLAGS += $(call if_coverage,--coverage)

# Project configuration ########################################################
RL_FILES  := $(wildcard *.rl)
RL_C      := $(RL_FILES:.rl=.c)
RL_O      := $(RL_FILES:.rl=.o)
CFILES    := $(RL_C) frob.c log.c utils.c serialization.c
OFILES    := $(CFILES:.c=.o)
UT_T      := $(wildcard *.in)
UT_C      := $(UT_T:.in=.c) utils.c serialization.c log.c
UT_O      := $(UT_C:.c=.o)
MUTATED_O := utils.o serialization.o
NORMAL_O  := $(RL_O) $(UT_T:.in=.o) log.o
ALL_C     := $(CFILES) $(UT_C)
ALL_PLIST := $(ALL_C:.c=.plist)

# Public targets ###############################################################
all: frob ut
index: tags cscope.out
test: test-unit test-functional test-random
clean:
	$(if $(strip $F),$(RM) -- $F)
coverage: test | $(CFILES) $(UT_C)
	gcov -o . $|
clang-analyze: $(ALL_PLIST)
tcp-server: frob | d5.txt
	s6-tcpserver -vd -b2 0.0.0.0 5002 s6-tcpserver-access -t200 -v3 -rp -B "Welcome!\r\n" $(realpath $<) 1000 $|
tcp-client: frob | d5.txt
	s6-tcpclient -rv localhost 5002 rlwrap $(realpath $<) 1000 $|
scan:
	scan-build $(MAKE) clean frob
graph-%: frame.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
protocol.png: protocol.rl protocol-adjust.sed
	ragel -p -V $(word 1,$^) | sed -Ef $(word 2,$^) | dot -Tpng -Gdpi=200 -o $@

# Internal targets #############################################################
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
acknak: acknak.o log.o utils.o
	$(LINK.o) -o $@ $^ $(LDLIBS)
	objcopy --only-keep-debug $@ $@.debug
	strip --strip-unneeded $@
	objcopy --add-gnu-debuglink=$@.debug $@
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
%.plist: %.c
	clang --analyze -o $@ $<
clean: F += $(wildcard $(RL_C) $(RL_C:.c=.s) $(UT_O) $(UT_T:.in=.c) $(UT_T:.in=.s) $(OFILES) frob frob.s log.s tags cscope.out ut)
clean: F += $(wildcard *.gcda *.gcno *.gcov)
clean: F += $(wildcard frob.log frob.sum frob.debug mut)
clean: F += $(wildcard $(ALL_PLIST))
clean: F += $(wildcard acknak.o)
