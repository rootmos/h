.PHONY: all
all: bpf lib

.PHONY: bpf
bpf:
	$(MAKE) -C bpf

lib: lib.c
	$(CC) -o "$@" "$<" -ldl

.PHONY: clean
clean:
	$(MAKE) -C bpf clean
