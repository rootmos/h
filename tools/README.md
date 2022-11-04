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

## Test tools
The `test-runner` script is this project's way of running tests.
For example running the `test-runner` in a subproject directory lists the
available tests. `test-runner` ran for [hlua](../hlua) suggests:

```
test/syntax
test/exit
test/require
test/noinput
test/runtime
test/hello
```

Then for example `test-runner test/hello` will run the test in the `test/hello`
directory.
When developing you may use the `--trace` option to run `strace` during the
test, which I find very useful while iterating to find the necessary syscalls
to allow in the seccomp filters.

Tests are defined and discovered by having a
[`test.toml`](../hlua/test/hello/test.toml) file:
```toml
cmdline = ["$0", "main.lua"]
```
where `"$0"` will be replaced by the relevant binary.
To test for [non-zero exit status](../hlua/test/exit/test.toml):
```toml
cmdline = ["$0", "main.lua"]
exit = 3
```
or if you expect to be [signaled](../hpython/test/socket/test.toml):
```toml
cmdline = ["$0", "echo_server.py"]
exit = "SIGSYS"
```

The subprojects' `Makefile`s have a `test` target that invokes the
`test-harness` script that runs all available tests and has the option to
bundle the test results into a tarball.
(The [Build and test](../.github/workflows/build-test.yaml)
workflow bundles and archives the test
results of the tests run against the system-wide installation.)
