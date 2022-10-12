.PHONY: all
all: bpf_asm bpf_dbg

bpf_asm bpf_dbg: build
	./$<

.PHONY: clean
clean:
	rm -f bpf_asm bpf_dbg
