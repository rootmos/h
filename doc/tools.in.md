# Build tools and utilities

## Berkeley Packet Filter tools
To make [cBPF](https://www.kernel.org/doc/Documentation/networking/filter.txt)
a feasible language for writing seccomp filters I have
bundled the kernel's
[`bpf_asm`](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/tools/bpf/bpf_asm.c)
with a simple "c-preprocessor-evalutaor";
[`pp`](pp) which evaluates c-expressions and constants:
such as:
```shell
pp SIGKILL # 9
pp "offsetof(struct seccomp_data, args[1])" # 24
```

The combined tool [`bpfc`](./bpfc) replaces literals prefixed with `$` and
expressions enclosed by `$$`:
```
jne #$__NR_fcntl, fcntl_end
ld [$$offsetof(struct seccomp_data, args[1])$$]
jeq #$F_GETFL, good
jmp bad
fcntl_end:
```

This makes writing reasonable programs (sic! the ability to reason about them)
easier (as well as more portable).
Compare with the
[kernel's seccomp example](https://www.kernel.org/doc/Documentation/networking/filter.txt):
```
** SECCOMP filter example:

  ld [4]                  /* offsetof(struct seccomp_data, arch) */
  jne #0xc000003e, bad    /* AUDIT_ARCH_X86_64 */
  ld [0]                  /* offsetof(struct seccomp_data, nr) */
  jeq #15, good           /* __NR_rt_sigreturn */
  jeq #231, good          /* __NR_exit_group */
  jeq #60, good           /* __NR_exit */
  jeq #0, good            /* __NR_read */
  jeq #1, good            /* __NR_write */
  jeq #5, good            /* __NR_fstat */
  jeq #9, good            /* __NR_mmap */
  jeq #14, good           /* __NR_rt_sigprocmask */
  jeq #13, good           /* __NR_rt_sigaction */
  jeq #35, good           /* __NR_nanosleep */
  bad: ret #0             /* SECCOMP_RET_KILL_THREAD */
  good: ret #0x7fff0000   /* SECCOMP_RET_ALLOW */
```
Can you resolve `AUDIT_ARCH_X86_64` to `0xc000003e`, or `__NR_mmap` in your head?

If your Linux distribution don't package `bpf_asm`: I provide a
[build script](bpf) that downloads the relevant sources and builds the `bpf`
tools.

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
@include "hlua.tests.txt"
```

Then for example `test-runner test/hello` will run the test in the `test/hello`
directory.
When developing you may use the `--trace` option to run `strace` during the
test, which I find very useful while iterating to find the necessary syscalls
to allow in the seccomp filters.

Tests are defined and discovered by having a
[`test.toml`](../hlua/test/hello/test.toml) file:
```toml
@include "../hlua/test/hello/test.toml"
```
where `"$0"` will be replaced by the relevant binary.
To test for [non-zero exit status](../hlua/test/exit/test.toml):
```toml
@include "../hlua/test/exit/test.toml"
```
or if you expect to be [signaled](../hpython/test/socket/test.toml):
```toml
@include "../hpython/test/socket/test.toml"
```

The subprojects' `Makefile`s have a `test` target that invokes the
`test-harness` script that runs all available tests and has the option to
bundle the test results into a tarball.
(The [Build and test](../.github/workflows/build-test.yaml)
workflow bundles and archives the test
results of the tests run against the system-wide installation.)
