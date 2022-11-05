ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

EXE ?= hsh
SRC ?= main.c

DEFAULT_SHELL ?= /bin/sh

CFLAGS += -DDEFAULT_SHELL=\"$(DEFAULT_SHELL)\"

.PHONY: build
build: $(EXE)

$(EXE).c: $(SRC) filter.bpfc landlock.filesc \
	capabilities.c seccomp.c version.c r.h
	$(SINGLE_FILE) -o "$@" "$<"

landlock.files: Makefile
	$(TOOLS)/poor_ldd "$(DEFAULT_SHELL)" "$@"

.PHONY: clean
clean:
	rm -f $(EXE) $(EXE).c *.filesc *.files *.bpfc version.c
