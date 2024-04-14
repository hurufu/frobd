CPPFLAGS     ?=
CFLAGS       ?=
TARGET_ARCH  ?=
SOURCES      := $(wildcard *.c coro/coro.c)
OBJECTS      := $(SOURCES:.c=.o)
DEPENDENCIES := $(if $(filter clean wipe,$(MAKECMDGOALS)),,$(SOURCES:.c=.d))

.PHONY: clean wipe
libcomulti.a: libcomulti.a($(OBJECTS))

include $(DEPENDENCIES)

%.d: %.c
	gcc $(CPPFLAGS) -MM -MF $@ -MT $*.o -MT $*.pic.o -o $@ $<
clean: REMOVABLE_FILES += $(OBJECTS) $(DEPENDENCIES) libcomulti.a
clean:
	$(let f,$(wildcard $(REMOVABLE_FILES)),$(if $(strip $f),$(RM) -- $f))
wipe: REMOVABLE_FILES += $(SOURCES:.c=.d)
wipe: clean
