ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

EXE ?= hsh
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE).c: $(SRC) filter.bpfc landlock.filesc \
	capabilities.c seccomp.c version.c r.h
	$(SINGLE_FILE) -o "$@" "$<"

.PHONY: clean
clean:
	rm -f $(EXE) $(EXE).c *.bpfc version.c
