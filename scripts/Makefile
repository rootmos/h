.PHONY: all
all: bpf_asm bpf_dbg

bpf_asm bpf_dbg: build_bpf_tools
	./$<

.PHONY: clean
clean:
	rm -f bpf_asm bpf_dbg
