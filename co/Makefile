EXECUTABLE        := demo
GENERATED_SOURCES := $(patsubst %.rl,%.c,$(wildcard *.rl))
LIB_SOURCES       := utils.c evloop.c coro.c
SOURCES           := comultitask.c main.c contextring.c $(GENERATED_SOURCES) $(LIB_SOURCES)
INDEXABLE_FILES   := comultitask.h frob.h contextring.h $(SOURCES)
CFLAGS            := -Wall -Wextra
CFLAGS            += -Og -ggdb3 -grecord-gcc-switches -Wformat -Werror=format-security
#CFLAGS            += -fvar-tracking-assignments -fasynchronous-unwind-tables
CFLAGS            += -gz=zlib -fdiagnostics-color=always -flto -ffat-lto-objects
#CFLAGS            += -fanalyzer
CPPFLAGS          := -DCORO_PTHREAD
#CPPFLAGS          += -DCORO_USE_VALGRIND
#CPPFLAGS          := -DNO_LOGS_ON_STDERR
LDFLAGS           := -Wl,-z,relro,-z,now,-z,defs
TARGET_ARCH       := -march=native -mtune=native
LDLIBS            :=
LOADLIBES         :=

vpath %.c .. ../coro
vpath %.txt ..

.PHONY: clean all run index print-% check

run: $(EXECUTABLE) | d5.txt
	./$< <$| | od -An -tx1z
check: ut
	./$<
ut: LDLIBS += -lcheck
ut: ut.c contextring.c
all: $(EXECUTABLE)
$(EXECUTABLE): $(SOURCES)
	$(LINK.c) -o $@ $^ $(LDLIBS)
%.c: %.rl
	ragel -L -G2 -o $@ $<
%.c: %.t
	checkmk <$< >$@
clean: removable_files := ut ut.c $(EXECUTABLE) $(GENERATED_SOURCES)
clean:
	$(let f,$(strip $(wildcard $(removable_files))),$(if $f,$(RM) -- $f))
index: tags cscope.out
tags: $(INDEXABLE_FILES)
	ctags -f$@ -R . ..
cscope.out: $(INDEXABLE_FILES)
	cscope -f$@ -bR
print-%:
	$(info $*=$($*))
