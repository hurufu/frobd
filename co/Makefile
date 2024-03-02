EXECUTABLE        := demo
GENERATED_SOURCES := $(patsubst %.rl,%.c,$(wildcard *.rl))
LIB_SOURCES       := utils.c evloop.c coro.c
SOURCES           := comultitask.c main.c $(GENERATED_SOURCES) $(LIB_SOURCES)
INDEXABLE_FILES   := comultitask.h frob.h $(SOURCES)
CFLAGS            := -Og -ggdb3 -grecord-gcc-switches -Wformat -Werror=format-security
CFLAGS            += -fvar-tracking-assignments -fasynchronous-unwind-tables
CFLAGS            += -gz=zlib -fdiagnostics-color=always -flto -ffat-lto-objects
CPPFLAGS          := -DCORO_PTHREAD -DCORO_USE_VALGRIND
LDFLAGS           := -Wl,-z,relro,-z,now,-z,defs
TARGET_ARCH       := -march=native -mtune=native
LDLIBS            :=

vpath %.c .. ../coro

.PHONY: clean all run index print-%

run: $(EXECUTABLE)
	echo hello | ./$<
all: $(EXECUTABLE)
$(EXECUTABLE): $(SOURCES)
	$(LINK.c) -o $@ $^ $(LDLIBS)
%.c: %.rl
	ragel -G2 -o $@ $<
clean: removable_files := $(EXECUTABLE) $(GENERATED_SOURCES)
clean:
	$(let f,$(strip $(wildcard $(removable_files))),$(if $f,$(RM) -- $f))
index: tags cscope.out
tags: $(INDEXABLE_FILES)
	ctags -f$@ -R . ..
cscope.out: $(INDEXABLE_FILES)
	cscope -f$@ -bR
print-%:
	$(info $*=$($*))
