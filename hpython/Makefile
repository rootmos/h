ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

PYTHON_PKG ?= python3-embed
CFLAGS += $(shell $(PKG_CONFIG) --cflags "$(PYTHON_PKG)")
LDFLAGS += $(shell $(PKG_CONFIG) --libs "$(PYTHON_PKG)")

EXE ?= hpython
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE).c: $(SRC) filter.bpfc r.h
	$(SINGLE_FILE) -o "$@" "$<"

.PHONY: clean
clean:
	rm -f $(EXE) $(EXE).c *.bpfc
