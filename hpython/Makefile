ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

TOOLS ?= $(TOOLS)/../scripts
BPFC ?= $(TOOLS)/bpf/bpfc
TCC_BUNDLER ?= $(TOOLS)/tcc-bundler

CC = gcc
TCC ?= 0
PKG_CONFIG ?= pkg-config

CFLAGS = -Wall -Werror -O2
LDFLAGS =
LOG_LEVEL ?= WARN
EXTRA_CFLAGS ?= -DLOG_LEVEL=LOG_$(LOG_LEVEL)
EXTRA_LDFLAGS ?=

PYTHON_PKG ?= python3-embed
CFLAGS += $(shell $(PKG_CONFIG) --cflags "$(PYTHON_PKG)")
LDFLAGS += $(shell $(PKG_CONFIG) --libs "$(PYTHON_PKG)")

EXE ?= hpython
SRC ?= main.c

.PHONY: build
build: $(EXE)

$(EXE): $(SRC) r.h filter.bpfc
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
