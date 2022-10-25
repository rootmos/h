ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

DESTDIR ?=
PREFIX ?= /usr

TOOLS ?= $(realpath $(ROOT)/../tools)
BPFC ?= $(TOOLS)/bpfc
PATHS ?= $(TOOLS)/paths
LANDLOCKC ?= $(TOOLS)/landlockc
VERSION ?= $(TOOLS)/version
SINGLE_FILE ?= $(TOOLS)/single-file
TEST_HARNESS ?= $(TOOLS)/test-harness

CC = gcc
CXX = g++
PKG_CONFIG ?= pkg-config

CFLAGS = -Wall -Werror -O2
LDFLAGS = -lcap
LOG_LEVEL ?= WARN
EXTRA_CFLAGS ?= -DLOG_LEVEL=LOG_$(LOG_LEVEL)
EXTRA_LDFLAGS ?=

.PHONY: all
all: build

.PHONY: test
test: build
	@$(TEST_HARNESS)

.PHONY: install
install: build
	install -sD "$(EXE)" "$(DESTDIR)$(PREFIX)/bin/$(EXE)"

%.bpfc: %.bpf
	$(BPFC) -i -o "$@" "$<"

%: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) $(EXTRA_LDFLAGS)

%: %.cpp
	$(CXX) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) $(EXTRA_LDFLAGS)

%.filesc: %.files
	$(LANDLOCKC) "$<" "$@"

version.c: $(VERSION) $(shell $(VERSION) -i)
	$(VERSION) -o "$@"
