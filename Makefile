CFLAGS=-I.  -g -Wall

bterm: term.c term.h os_lin.c cmds/cmds_test.c
	@echo BUILDING $@
	@gcc ${CFLAGS} $^ -o $@
