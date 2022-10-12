ROOT := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

TOOLS ?= $(realpath $(ROOT)/../tools)
BPFC ?= $(TOOLS)/bpfc
TCC_BUNDLER ?= $(TOOLS)/tcc-bundler

CC = gcc
PKG_CONFIG ?= pkg-config

CFLAGS = -Wall -Werror -O2
LDFLAGS = -llua
LOG_LEVEL ?= WARN
EXTRA_CFLAGS ?= -DLOG_LEVEL=LOG_$(LOG_LEVEL)
EXTRA_LDFLAGS ?=

%.bpfc: %.bpf
	$(BPFC) -o "$@" "$<"
