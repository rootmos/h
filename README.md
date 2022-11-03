# *h*ardened script host programs
[![Build and test](https://github.com/rootmos/h/actions/workflows/build-test.yaml/badge.svg)](https://github.com/rootmos/h/actions/workflows/build-test.yaml)

Unprivileged sandboxing of popular script languages.

|Unprivileged|sandboxing|script languages|
|------------|----------|----------------|
|Alice: "Why not Docker?"<br>Bob: Because `sudo docker`.|[landlock](https://www.kernel.org/doc/html/latest/userspace-api/landlock.html) <br> [seccomp](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)|[lua](http://lua.org/) -> [hlua](hlua) <br> [python](https://python.org/) -> [hpython](hpython) <br> [node](https://nodejs.org/) -> [hnode](hnode)|

## DISCLAIMER
This project is a work in progress and has not been audited by security
experts.

However I think it it remains useful for educational purposes regarding Linux's
sometimes daunting security features and using `strace` to illustrate how a
program written in a high level language is translated into syscalls to obtain
its desired (or undesired) effects.

... and it is certainly better than nothing as I will try to exemplify in the
following section. But as always, remember that sandboxing and containers (yes
I'm looking at you Docker fans): only limit the extent of a successful attack,
and don't give you carte blanche for you to willy-nilly execute untrusted code.

## Raison d'être
So given that disclaimer, why did I write this?
The secondary goal is to show of Linux's security features in a
concrete setting, but my primary my goal is for the reader to have added
`strace` to her list of favorite tools.

### Alice's game
Assume Alice is a game designer with malicious intents and you are her intended
victim.
Being a fan of indie games you, of course, accept to be a beta-tester for her
latest creation.
So Alice sends you the `fun.lua` game, hidden within is the statement:
```lua
os.execute("sudo rm -r /")
```
(or maybe she'll try `sudo --askpass` if the credentials aren't cached).
A diligent code-reviewer might catch such an obviously malicious
statement, but such a statement can be surprisingly easy to miss in a hurried
glance:
```lua
function run(cmdline)
    local s = os.getenv("SUDO")
    if not s then
        cmdline = "sudo -A " .. cmdline
    end
    os.execute(cmdline)
end

function clean_cache()
    local project = os.getenv("PROJECT_ROOT") or ".."
    local cache = os.getenv("CACHE") or "/tmp"
    run(string.format("rm -r %s/%s", cache, project))
end
```
Then there are programming languages
[designed to be difficult to read](https://esolangs.org/wiki/Esoteric_programming_language#Obfuscation).
And speaking of programming languages: "the greatest thing about Lua is that
you don't have to write Lua."
Meaning that it's very feasible to bundle a compiler for another language
however esoteric (check out: [fennel](https://fennel-lang.org) and
[Amulet](https://amulet.works/)).
But Lua (as well as python, node, c and many many more) are:
any-effect-any-time languages.
This in contrast with [Haskell](https://www.haskell.org)
(check out [Learn You a Haskell for Great Good!](http://www.learnyouahaskell.com))
or [eff](https://www.eff-lang.org/) if you're feeling adventurous.
That means that an expected pure/side-effect free operation such as compiling a
piece of source code can execute the above `os.execute`-attack, or worse if the
attacker has an insidious mind.
And considering that compilers are usually quite extensive pieces of software
they provide ample forestry to hide a malicious tree.
Alice, I suggest you split the malicious code in several commits/PR:s.
For the victim, I recommend [Ken Thompson's "Reflections on Trusting Trust"](https://dl.acm.org/doi/10.1145/358198.358210),
which if you haven't read I expect will shatter any trust you might have
imagined you had in *any* binary executable.

So the world is a scary and unsatisfactory environment, so let's begin
mitigating the consequences of malicious or incompetently written code.

### No new privileges
Alice's `sudo`-based `rm -f /`-attack can be mitigated by a one-liner:
[`prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)`](https://man.archlinux.org/man/prctl.2#PR_SET_NO_NEW_PRIVS).
This call is not expected to fail, but being a conscientious developer it never
hurts to crash-don't-thrash and I present a copy-pastable snippet:
```c
#include <sys/prctl.h>
#include <stdlib.h>

void no_new_privs(void)
{
    if(0 != prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        abort();
    }
}
```
You might have used (or prefer) [`exit`](https://man.archlinux.org/man/exit.3a).
I don't: because libc:s commonly provide
[`atexit`](https://man.archlinux.org/man/atexit.3)
which in my opinion is contrary to a fail-early/crash-don't-thrash philosophy:
the operating system already have to assume the responsibility of clean up
after a failing process.
(Ever noticed that c coders don't free their allocations when exiting?)
Using `exit` and `atexit` reminds me of exception handling and the nightmare
when exception handlers raise exceptions.
Instead consider programming models where failure-is-always-an-option thinking
is prevalent,
such as the actor model where the non-delivery of a message is a scenario
brought to the forefront (the real-world scenario is the fallibility of network
connections). If you are curious I recommend [Erlang](https://www.erlang.org/)
(check out [Learn You Some Erlang for great good!](https://learnyousomeerlang.com/)).

Back to mitigations: the above `no_new_privs` function call is so simple
it should always be used.
Unless *explicitly necessary* to gain new privileges.
This is the [Principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege):
if the functionality you intend to provide do not require privileges your
process should not have any privileges, and this is the red thread in this
attempt at raison d'être.
But in the [RealWorld](https://hackage.haskell.org/package/base-4.13.0.0/docs/Control-Monad-ST-Safe.html#t:RealWorld),
processes inherit quite a handful of privileges that Alice can still abuse,
as we shall see.

So Alice can't `sudo` anymore thanks to `PR_SET_NO_NEW_PRIVS`,
but even a sneaky `os.execute("rm -r ~")` would still
be a [mayor](https://www.youtube.com/watch?v=fmAWIDI4ZgY) buzzkill.

The naive Lua specific mitigation is to `os.execute = nil` before running the
entrypoint of Alice's game.
Well, that may be good enough
(I haven't figured out a way around that mitigation, but I'm reasonably sure
there may exist an exploit and would be interested in seeing it).
Continuing this idea we can tweak it into at least making this first naive
mitigation useful:
```lua
os.execute = function() error("not allowed") end
```
Especially since the mitigation I suggest below produces a far less
user-friendly error message.

### Enter [seccomp](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)
Seccomp is Linux's way of filtering syscalls and so limiting the exposed
kernel surface.

Fancy words aside, this means that when you receive in your email inbox the
notification of a new vulnerability you can feel certain that you are not
affected because the vulnerable syscalls are rejected by your program.

The simplest seccomp filters are essentially accept/reject lists, but can do
more complex things.
But when it comes to security: easily read and understandable code is always
preferred.

Back to Alice's `os.execute`-based attacks:
with seccomp enabled with a filter that forbids `exec`:s,
the kernel will politely kill your process and suggest to the rest of the
system that you received a `SIGSYS` signal.
In practice this means that your process immediately vanish, so without a
syscall inspection tool such as `strace` one is reduced to debugging by: "thou
shalt printf".

#### [strace](https://man.archlinux.org/man/strace.1)
If you haven't invoked strace before, or you are curious what syscalls are
being used by a program then try:
```shell
strace lua -e 'print("hello")'
strace python -c 'print("hello")'
```
The output of `strace` can be quite extensive (and therefore `strace` provides
sophisticated ways to filter what is traced).
For our [hello world](https://rosettacode.org/wiki/Hello_world/Text) example
the interesting syscall can be found towards the end:
```
write(1, "hello\n", 6)                  = 6
```
Other interesting syscalls to look for are memory allocating syscalls such as
[`brk`](https://man.archlinux.org/man/brk.2) and
[`mmap`](https://man.archlinux.org/man/mmap.2):
```
mmap(NULL, 151552, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f55a8b74000
```

Continuing our investigation of Alice's `os.execute`-attack, we can trace an
almost as trivial piece of code:
```shell
strace -f lua -e 'os.execute("echo hello")'
strace -f python -c 'import os; os.system("echo hello")'
```
Note the `-f` (`--follow-forks`) option that tells `strace` continue tracing
spawned child processes.
And now we look for
[`clone`](https://man.archlinux.org/man/clone.2) (the syscall that implements
[`fork`](https://man.archlinux.org/man/fork.2))
and [`execve`](https://man.archlinux.org/man/execve.2).
From the trace in the parent:
```
clone3({flags=CLONE_VM|CLONE_VFORK, exit_signal=SIGCHLD, stack=0x7f8160953000, stack_size=0x9000}, 88) = 2304236
```
and in the child:
```
execve("/bin/sh", ["sh", "-c", "echo hello"], 0x7ffebf549aa8 /* 52 vars */) = 0
```

So why not reject `clone` as well?
Remember, in Linux threads and processes are the same abstraction, one with
shared virtual memory space and the other not.
Now both thread as well as processes are no longer a thing you need to reason
about.

#### [Berkeley Packet Filter](https://www.kernel.org/doc/html/latest/bpf/index.html)
Seccomp filters are expected to be binary representations of
[cBPF](https://www.kernel.org/doc/Documentation/networking/filter.txt), the c
stands for "classic" BPF (in contrast with
extended BPF ([eBPF](https://www.kernel.org/doc/html/latest/bpf/index.html)).
While cBPF is not theoretically Turing complete
because of lack of infinite memory; restricted to the scratch memory:
`uint32_t M[16]`.
That only present an interesting challenge:
which [Project Euler](https://projecteuler.net) or
[Advent of Code](https://adventofcode.com) problems can be solved in cBPF?

Therefore working with seccomp can provide somewhat of a challenge.
So you may want to use an
[assembler](https://github.com/torvalds/linux/blob/master/tools/bpf/bpf_asm.c)
and a [preprocessor](tools/pp) (I've bundled them together as [bpfc](tools/bpfc))
that can interpret the constants commonly used
when making syscalls.

I always start with a "reject everything" filter:
```
bad: ret #$SECCOMP_RET_KILL_THREAD
good: ret #$SECCOMP_RET_ALLOW
```
then run a test under `strace`, look for `SIGSYS`, reason about the offending
syscall, reluctantly add it to the allowed-list and iterate:
```
ld [$$offsetof(struct seccomp_data, nr)$$]

jeq #$__NR_brk, good

bad: ret #$SECCOMP_RET_KILL_THREAD
good: ret #$SECCOMP_RET_ALLOW
```
Eventually when the test pass you have achieved a list of syscalls
living up to the [principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege).

Doing this dance for some non-trivial test-cases you end up with a filter
usually including the: `write`, `read`, `close` file descriptor manipulating
functions.

`write` is particularly fun to think about: without it how can you communicate
the result of any computation in an "everything is a file" system?
The syscall filtering way of expressing this thought is seccomp's strict mode:
only allow `write` and `exit`. The reasoning being is that you are only allowed
to `write` to *already opened* file descriptors. (Note that in this setting
`open` is forbidden, or more accurately not expressively allowed.)

But even moderately interesting Lua application enjoys using `require`. So it's
not unreasonable to allow Lua to `open` files. But then Alice changes her
`fun.lua` game to include (obfuscated):
```lua
io.open(os.getenv("HOME") .. "/.aws/credentials", "r"):read("*a")
```
Now Alice has to get this information back to her, but maybe it's a
multiplayer game? Or she obfuscates it in the game's log file and exclaims:
"Oh the game crashed, why don't you send me the logs?"

### Enter [landlock](https://www.kernel.org/doc/html/latest/userspace-api/landlock.html)
Landlock is a fairly recently added security feature, which is meant to
restrict filesystem access for unprivileged processes, in addition to the
standard UNIX file permissions.
In essence landlock grants or restricts rights to filesystem operations
on whole filesystem hierarchies. (Note that a single file is a trivial
hierarchy.)
So we can grant read access to `/usr/lib` only and mitigate Alice's attack on
your access tokens in your home directory. And maybe allow both read and write
to `/tmp`, and maybe allow removing (i.e. unlinking). (Unless you allow `open`'s
`O_TMPFILE` in your seccomp filter of course.)

### Drop capabilities
Lastly I have included a code snippet to drop all capabilities. This is a Linux
feature I previously hadn't had the need to explore (so take that code and what
comes next with a grain of salt and trust, but verify). The classic selling
point of capabilities is the scenario to allow unprivileged users to run
`ping`. In a pre-capabilities world one would have to have to obtain the full
power of the privileged user (read: `root`) in order to use `ping`. Of course
`setsuid` reduces the mess of every user `su`:ing, but still provides a nice
potential attack vector on the `ping` binary.
The capabilities is basically the idea to split `root` into separate, well,
capabilities that can be granted independently.
(`ping` requires the `CAP_NET_RAW` capability).

In this project this scenario isn't really applicable. But what is applicable
is the functionality to relinquish  granted capabilities from the current
process.
Maybe this sounds convoluted, but in our current Dockerized world I would say
its fairly common to see images invoke executables in a privileged mode
(as `root`).

And noteworthy configuration option of Linux is that you don't have to include
the bothersome userland? Here I imagine a barebones server setup: the kernel,
a single stand-alone server executable and nothing else. So in that setting
dropping all capabilities could be useful.

## Installation and build instructions

### Building from sources
This project is intended to be built using `Makefile`:s and common `c` build
tools, except for the `bpf_asm` tool (found in the
[Linux kernel sources](https://github.com/torvalds/linux/tree/master/tools/bpf)).
Arch Linux users can use the [`bpf`](https://archlinux.org/packages/community/x86_64/bpf/)
package, but other distributions might have to build their own copies.
I have prepared a [build script](tools/bpf/build) which is used when `bpf_asm`
is not found (the script is used in the Ubuntu CI-job).

The steps to build the project is then:
```shell
make tools
make build
make check
```

If these steps fail because of missing dependencies you may consult the
following table (derived from the packages installed during the
[Build and test](.github/workflows/build-test.yaml) workflow).

| | runtime | build | check |
|-|---------|-------|-------|
|Ubuntu 22.04| `libcap2 lua5.4 python3 libnode72` | `make pkg-config gcc libcap-dev wget ca-certificates bison flex liblua5.4-dev python3 libpython3-dev libnode-dev` | `uuid-runtime jq` |
|Arch Linux| `lua python nodejs` | `make gcc pkgconf bpf` | `jq` |

### Building from a Ubuntu source package
Pick a release and download the Ubuntu source package asset.
Included within are the sources and two helper scripts:
- `build-package` that runs
  [`dpkg-buildpackage`](https://manpages.ubuntu.com/manpages/jammy/en/man1/dpkg-buildpackage.1.html),
  as well as checking for missing build-time dependencies
- `install-package` installs the built package using `apt-get`, but note that
  you can try out the built binaries without a system-wide installation

These scripts are intended to be running as an unprivileged user, but might
need `sudo` access to `apt-get` in order to install missing dependencies.
Both scripts accept an `-s` option for this case, or you can set the `SUDO`
environment variable (e.g. `SUDO="sudo --askpass"`).

### Building from an Arch Linux PKGBUILD
Pick a release and download the Arch Linux
[`PKGBUILD`](https://wiki.archlinux.org/title/PKGBUILD)
asset, place it in a suitably empty directory and invoke
[`makepkg`](https://wiki.archlinux.org/title/PKGBUILD),
possibly with `--syncdeps` and/or `--install` options when desired.
Note that you can try out the built binaries (found in the created `src`
subfolder) without a system-wide installation.

## TODO
- [ ] landlock ABI=2 (see the [sandbox example](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/samples/landlock/sandboxer.c#n233))
- [ ] [readline](https://en.wikipedia.org/wiki/GNU_Readline) or naive
  [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop)
  with [rlwrap](https://github.com/hanslub42/rlwrap)
