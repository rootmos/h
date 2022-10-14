ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

PREFIX ?= /usr

TOOLS ?= $(realpath $(ROOT)/../tools)
BPFC ?= $(TOOLS)/bpfc
PATHS ?= $(TOOLS)/paths
LANDLOCKC ?= $(TOOLS)/landlockc
SINGLE_FILE ?= $(TOOLS)/single-file
TEST_HARNESS ?= $(TOOLS)/test-harness

CC = gcc
PKG_CONFIG ?= pkg-config

CFLAGS = -Wall -Werror -O2
LDFLAGS =
LOG_LEVEL ?= WARN
EXTRA_CFLAGS ?= -DLOG_LEVEL=LOG_$(LOG_LEVEL)
EXTRA_LDFLAGS ?=

%.bpfc: %.bpf
	$(BPFC) -i -o "$@" "$<"

%: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) $(EXTRA_LDFLAGS)

%.filesc: %.files
	$(LANDLOCKC) "$<" "$@"

.PHONY: all
all: build

.PHONY: test
test: build
	@$(TEST_HARNESS)

.PHONY: install
install: build
	install -sD "$(EXE)" "$(PREFIX)/bin/$(EXE)"
