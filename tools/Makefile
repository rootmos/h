CFLAGS = -Wall -Werror -O2
LDFLAGS =
LOG_LEVEL ?= WARN
EXTRA_CFLAGS ?= -DLOG_LEVEL=LOG_$(LOG_LEVEL)
EXTRA_LDFLAGS ?=

.PHONY: all
all: bpf lib capture

.PHONY: bpf
bpf:
	$(MAKE) -C bpf

lib: lib.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) -ldl $(EXTRA_LDFLAGS)

capture: capture.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o "$@" "$<" $(LDFLAGS) $(EXTRA_LDFLAGS)

.PHONY: clean
clean:
	rm -f lib capture
	$(MAKE) -C bpf clean
