ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
include $(ROOT)/../build/common.makefile

PYTHON_PKG ?= python3-embed
CFLAGS += $(shell $(PKG_CONFIG) --cflags "$(PYTHON_PKG)")
LDFLAGS += $(shell $(PKG_CONFIG) --libs "$(PYTHON_PKG)")

EXE ?= hpython
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE).c: $(SRC) filter.bpfc landlock.filesc \
	capabilities.c seccomp.c version.c r.h
	$(SINGLE_FILE) -o "$@" "$<"

landlock.files: Makefile
	$(PATHS) -o"$@" --python-site -lz

.PHONY: clean
clean:
	rm -f $(EXE) $(EXE).c *.bpfc *.filesc *.files version.c
