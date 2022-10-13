ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

TOOLS ?= $(realpath $(ROOT)/../tools)
BPFC ?= $(TOOLS)/bpfc
SINGLE_FILE ?= $(TOOLS)/single-file

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
