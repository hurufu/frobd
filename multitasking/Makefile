CPPFLAGS     ?=
CFLAGS       ?=
TARGET_ARCH  ?=
SOURCES      := coro/coro.c contextring.c eventloop.c suspendables.c
OBJECTS      := $(SOURCES:.c=.o)
DEPENDENCIES  = $(if $(filter clean wipe,$(MAKECMDGOALS)),,$(SOURCES:.c=.d) ut.d)
LOADLIBES    :=

.PHONY: clean wipe check all
all: libcomulti.a
check: ut
	./$<
clean: REMOVABLE_FILES += $(OBJECTS) $(DEPENDENCIES) libcomulti.a ut.c
clean:
	$(let f,$(wildcard $(REMOVABLE_FILES)),$(if $(strip $f),$(RM) -- $f))
wipe: REMOVABLE_FILES += $(SOURCES:.c=.d) ut.d
wipe: clean

libcomulti.a: libcomulti.a($(OBJECTS))
ut: LDLIBS  += -lcheck
ut: LDFLAGS ?=
ut: ut.o contextring.o

include $(DEPENDENCIES)

%.d: %.c
	gcc $(CPPFLAGS) -MM -MF $@ -MT $*.o -MT $*.pic.o -o $@ $<
%.c: %.c.t
	checkmk <$< >$@
