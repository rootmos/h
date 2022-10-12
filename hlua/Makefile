ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

LUA_PKG ?= lua
CFLAGS += $(shell $(PKG_CONFIG) --cflags "$(LUA_PKG)")
LDFLAGS += $(shell $(PKG_CONFIG) --libs "$(LUA_PKG)")

EXE ?= hlua
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE): $(SRC) filter.bpfc r.h
ifeq ($(TCC), 1)
	$(TCC_BUNDLER) $(LDFLAGS) -o "$(@)" "$<"
else
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) $(EXTRA_LDFLAGS)
endif

%.bpfc: %.bpf
	$(BPFC) -o "$@" "$<"

.PHONY: clean
clean:
	rm -f "$(EXE)" *.bpfc
