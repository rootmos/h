CC = gcc

CFLAGS = -Wall -Werror -O2
LDFLAGS =
LOG_LEVEL ?= WARN
EXTRA_CFLAGS ?= -DLOG_LEVEL=LOG_$(LOG_LEVEL)
EXTRA_LDFLAGS ?=

.PHONY: all
all: bpf lib capture strace-log-watch

.PHONY: bpf
bpf:
	$(MAKE) -C bpf

lib: lib.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) -ldl $(EXTRA_LDFLAGS)

%: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) $(EXTRA_LDFLAGS)

.PHONY: clean
clean:
	rm -f lib capture strace-log-watch
	$(MAKE) -C bpf clean
