ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

SCRIPTS ?= $(ROOT)/../scripts
BPFC ?= $(SCRIPTS)/bpfc
TCC_BUNDLER ?= $(SCRIPTS)/tcc-bundler

CC = gcc
TCC ?= 0

CFLAGS = -Wall -Werror -O2
LDFLAGS = -llua
LOG_LEVEL ?= WARN
EXTRA_CFLAGS ?= -DLOG_LEVEL=LOG_$(LOG_LEVEL)
EXTRA_LDFLAGS ?=

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
