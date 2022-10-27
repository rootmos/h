ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

CFLAGS += -I/usr/include/node
LDFLAGS += -lnode

CFLAGS += $(shell $(PKG_CONFIG) --cflags libuv)
LDFLAGS += $(shell $(PKG_CONFIG) --libs libuv)

EXE ?= hnode
SRC ?= main.cpp

.PHONY: build
build: $(EXE)

$(EXE).cpp: $(SRC) capabilities.c version.c r.h main.jsc
	$(SINGLE_FILE) -o "$@" "$<"

%.jsc: %.js
	$(NULL_TERMINATED) -o "$@" "$<"

.PHONY: clean
clean:
	rm -f $(EXE) $(EXE).cpp *.bpfc version.c *.jsc
