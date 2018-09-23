CFLAGS=-I.  -g -Wall

bterm: term.c term.h os_lin.c cmds/cmds_test.c
	@echo CC $@
	@gcc ${CFLAGS} $^ -o $@

clean:
	@echo CLEAN
	@rm -f bterm


.PHONY: clean
