# BPF tools build helper
The [build](build) helper script will either find the
[BPF](https://www.kernel.org/doc/Documentation/networking/filter.txt) tools
(in particular `bpf_asm`) in the `PATH` or download the source code from the
Linux kernel and tries to build them.
