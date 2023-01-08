.PHONY: index clean graph-% run run-%

CFLAGS ?= -O3 -g0 -Wall -Wextra

run: run-01 run-02
run-%: main sample%
	./$< <$(word 2,$^)
index: tags cscope.out
tags:
	ctags -R .
cscope.out:
	cscope -bR

main: main.c frob-msg.c
	$(LINK.c) -o $@ $^ $(LDLIBS)
%.c: %.rl
	ragel -G2 $<
%.s: %.c
	$(CC) -S -o $@ -fverbose-asm -fno-asynchronous-unwind-tables $(CFLAGS) $<

graph-%: frob-msg.rl adjust-%.sed
	ragel -p -V $< | sed -Ef $(word 2,$^) | dot -Tpng | feh -
clean: F += frob-msg.c main main.s frob-msg.s tags cscope.out
clean:
	$(if $(strip $(wildcard $F)),$(RM) -- $(wildcard $F))
