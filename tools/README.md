# Build tools and utilities

## BPF tools
[`pp`](pp) is a tool that evaluates c expressions and constants.
For example `pp SIGKILL` spits out `9`, but more useful are expressions with
`offsetof` and `sizeof`: `pp "offsetof(struct seccomp_data, args[1])"`.

I use this script together with `bpf_asm` to write more understandable and
maintainable BPF programs: the [`bpfc`](bpfc) script inputs human readable BPF
assembler and outputs a c-snippet with the BPF opcodes.
The `bpf_asm` is the BPF assembler included in the Linux kernel source code,
and I have included a [build](bpf) helper for getting the source code and
building the tool.

## Path resolution tools
Since landlock is based on paths to restrict filesystem access its desirable to
figure out at build-time the relevant paths a script system will access.
A good example is Python's site-packages directory:
`paths --python-site` outputs `/usr/lib/python3.10` for me.
Python also sometimes wants to load shared libraries, for instance `libz`, so
what path should landlock give read access to?
`paths -lz` gives me `/usr/lib/libz.so.1`.

Then the `landlockc` tool will take this list of paths and generates a
c-snippet that grants the relevant read accesses.
