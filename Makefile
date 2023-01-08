.PHONY: index clean graph-% run run-%

CFLAGS := -O0 -ggdb3 -Wall -Wextra -ffat-lto-objects -mtune=native -march=native
LDFLAGS := -flto

run: run-01 run-02 run-T4
run-%: main sample%
	./$< <$(word 2,$^)
index: tags cscope.out
tags:
	ctags -R .
cscope.out:
	cscope -bR

main: main.c frob-frame.c frob-header.c frob-protocol.c
	$(LINK.c) -o $@ $^ $(LDLIBS)
%.c: %.rl
	ragel -G2 -L $<
%.s: %.c
	$(CC) -S -o $@ -fverbose-asm -fno-asynchronous-unwind-tables $(CFLAGS) $<

graph-%: frob-frame.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
clean: F += frob-protocol.c frob-frame.c frob-msg.c frob-header.c main main.s frob-frame.s frob-header.s frob-msg.s tags cscope.out frob-protocol.s
clean:
	$(if $(strip $(wildcard $F)),$(RM) -- $(wildcard $F))
