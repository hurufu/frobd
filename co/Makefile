EXECUTABLE        := demo
GENERATED_SOURCES := $(patsubst %.rl,%.c,$(wildcard *.rl))
LIB_SOURCES       := utils.c evloop.c coro.c
SOURCES           := comultitask.c main.c $(GENERATED_SOURCES) $(LIB_SOURCES)
INDEXABLE_FILES   := comultitask.h frob.h $(SOURCES)
CFLAGS            := -Os
CPPFLAGS          :=
LDFLAGS           :=
TARGET_ARCH       := -march=native -mtune=native
LDLIBS            :=

vpath %.c .. ../coro

.PHONY: clean all run index

run: $(EXECUTABLE)
	./$<
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
	ctags -f$@ -R .
cscope.out: $(INDEXABLE_FILES)
	cscope -f$@ -bR
