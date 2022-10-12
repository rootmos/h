ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

LUA_PKG ?= lua
CFLAGS += $(shell $(PKG_CONFIG) --cflags "$(LUA_PKG)")
LDFLAGS += $(shell $(PKG_CONFIG) --libs "$(LUA_PKG)")

EXE ?= hlua
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE).c: $(SRC) filter.bpfc r.h
	$(SINGLE_FILE) -o "$@" "$<"

.PHONY: clean
clean:
	rm -f $(EXE) $(EXE).c *.bpfc
