EXECUTABLE        := demo
GENERATED_SOURCES := $(patsubst %.rl,%.c,$(wildcard *.rl))
LIB_SOURCES       := utils.c evloop.c coro.c
SOURCES           := comultitask.c main.c $(GENERATED_SOURCES) $(LIB_SOURCES)

vpath %.c .. ../coro

.PHONY: clean all

all: $(EXECUTABLE)
$(EXECUTABLE): $(SOURCES)
	$(LINK.c) -o $@ $^ $(LDLIBS)
%.c: %.rl
	ragel -G2 -o $@ $<
clean: removable_files := $(EXECUTABLE) $(GENERATED_SOURCES)
clean:
	$(let f,$(strip $(wildcard $(removable_files))),$(if $f,$(RM) -- $f))
