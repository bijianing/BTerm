CFLAGS=-I.  -g -Wall

SRC=term.c os_lin.c cmds/cmds_test.c
bterm: $(SRC) term.h
	@echo CC $@
	@gcc ${CFLAGS} $(SRC) -o $@

clean:
	@echo CLEAN
	@rm -f bterm


.PHONY: clean
