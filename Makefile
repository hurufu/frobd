.PHONY: index clean graph-% test tcp scan coverage all test-unit test-functional test-random test-mutation mutated clang-analyze

OPTLEVEL := g

PROJECT_DIR := $(firstword $(dir $(shell realpath --relative-to=. $(MAKEFILE_LIST))))

# Compiler configuration #######################################################
if_coverage = $(if $(findstring coverage,$(MAKECMDGOALS)),$(1),)

CPPFLAGS_clang := -D_FORTIFY_SOURCE=3
# Some gcc builds (depends on the distro) set _FORTIFY_SOURCE by default,
# so we need undefine it and then redefine it
CPPFLAGS_gcc   := -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS
CPPFLAGS_cc     = $(CPPFLAGS_gcc)
CPPFLAGS       ?= $(CPPFLAGS_$(CC))
CPPFLAGS       += -I$(PROJECT_DIR) -I$(PROJECT_DIR)multitasking
CPPFLAGS       += -D_GNU_SOURCE
#CPPFLAGS       += -DNDEBUG
# Disable all logs
#CPPFLAGS       += -DNO_LOGS_ON_STDERR

CFLAGS_gcc := -fanalyzer -fanalyzer-checker=taint -fsanitize=bounds -fsanitize-undefined-trap-on-error -ffat-lto-objects
CFLAGS_gcc += -Wformat -Werror=format-security -grecord-gcc-switches
CFLAGS_gcc += -fstack-protector-all
CFLAGS_gcc += -fstack-clash-protection
CFLAGS_cc   = $(CFLAGS_gcc)
CFLAGS     ?= -std=gnu11 -O$(OPTLEVEL) -ggdb3 -Wall -Wextra -flto $(CFLAGS_$(CC))
CFLAGS     += $(call if_coverage,--coverage)
#CFLAGS   += -fstrict-flex-arrays
# TODO: Remove those warnings only for generated files
CFLAGS     += -Wno-implicit-fallthrough -Wno-unused-const-variable -Wno-sign-compare -Wno-unused-variable -Wno-unused-parameter
TARGET_ARCH := -mtune=native -march=native 
LDLIBS     := -lnpth

LDFLAGS ?= -flto
LDFLAGS += $(call if_coverage,--coverage)

# Project configuration ########################################################
RL_C      := wireprotocol.c header.c body.c attrs.c frame.c
RL_O      := $(RL_C:.c=.o)
CFILES    := main.c log.c utils.c serialization.c ucspi.c npthfix.c
OFILES    := $(RL_O) $(CFILES:.c=.o)
UT_C      := ut.c serialization.c log.c utils.c contextring.c
UT_O      := $(UT_C:.c=.o) header.o body.o frame.o attrs.o
MUTATED_O := utils.o serialization.o
NORMAL_O  := $(RL_O) ut.o log.o
ALL_C     := $(CFILES) $(UT_C)
ALL_PLIST := $(ALL_C:.c=.plist)

HOSTNAME  := $(shell hostname -f)
BUILD_DIR ?= build

vpath %.rl  $(PROJECT_DIR)fsm
vpath %.sed $(PROJECT_DIR)fsm
vpath %.c   $(PROJECT_DIR) $(addprefix $(PROJECT_DIR),multitasking multitasking/coro)
vpath %.t   $(PROJECT_DIR)
vpath %.txt $(PROJECT_DIR)

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
	s6-tcpclient -rv localhost 5002 $(realpath $<) 1000 $|
tls-server: frob server.cer server.key | d5.txt
	env -i PATH=/bin CERTFILE=$(word 2,$^) KEYFILE=$(word 3,$^) s6-tlsserver 0.0.0.0 6666 $(realpath $<) -f $(word 1,$^)
tls-client: frob server.cer | d5.txt
	env -i PATH=/bin CAFILE=$(word 2,$^) s6-tlsclient 0.0.0.0 6666 $(realpath $<) -f $(word 1,$^)
scan:
	scan-build $(MAKE) clean frob
show-%: %.png
	feh - <$< &
%.png: %.dot
	dot -Tpng $< >$@
%.dot: %.rl %.sed
	ragel -V $< | sed -Ef $(word 2,$^) >$@
graph-%: frame.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
protocol.png: protocol.rl protocol-adjust.sed
	ragel -p -V $(word 1,$^) | sed -Ef $(word 2,$^) | dot -Tpng -Gdpi=200 -o $@
first_level.png: first_level.rl first_level-adjust.sed
	ragel -p -V $(word 1,$^) | sed -Ef $(word 2,$^) | dot -Tpng -Gdpi=200 -o $@
dir-%: | $(BUILD_DIR)/
	+$(MAKE) -C $| $(addprefix -f ,$(abspath $(MAKEFILE_LIST))) $*

# Internal targets #############################################################
tags:
	ctags --kinds-C=+p -R . /usr/include/{sys,bits,}/*.h /usr/local/src/npth
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
	ragel -G2 -L -o $@ $<
%.s: %.c
	$(CC) -S -o $@ -fverbose-asm -fno-asynchronous-unwind-tables $(CFLAGS) -fno-lto $<
%.c: %.t
	checkmk $< >$@
%.plist: %.c
	clang --analyze -o $@ $<
%.key:
	openssl genpkey -algorithm RSA -out $@ -pkeyopt rsa_keygen_bits:4096
%.cer: %.key
	openssl req -new -x509 -sha256 -key $< -out $@ -days 2 -subj '/L=$(word 2,$(subst /, ,$(TZ)))/CN=$(HOSTNAME)'
%/:
	mkdir -p -- $@
clean: F += $(wildcard $(RL_C) $(RL_C:.c=.s) $(UT_O) ut.c ut.s ut.o $(OFILES) frob frob.s log.s tags cscope.out ut)
clean: F += $(wildcard *.gcda *.gcno *.gcov)
clean: F += $(wildcard frob.log frob.sum frob.debug mut)
clean: F += $(wildcard $(ALL_PLIST))
clean: F += $(wildcard evloop.o evloop.debug evloop.s evloop)
clean: F += $(wildcard *.key *.cer)
