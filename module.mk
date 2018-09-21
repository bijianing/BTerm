

include $(CLEAR_LOCAL_VARIABLE)
local_module   := BTerm
local_sources  := term.c os_tk.c cmds/cmds_net.c
local_headers  := term.h
local_includes :=
local_cflags   :=
include $(CREATE_LIBRARY)

