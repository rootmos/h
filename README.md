# *h*ardened script host programs
[![Build and test](https://github.com/rootmos/h/actions/workflows/build-test.yaml/badge.svg)](https://github.com/rootmos/h/actions/workflows/build-test.yaml)

Unprivileged sandboxing of popular script languages.

|Unprivileged|sandboxing|script languages|
|------------|----------|----------------|
|Alice: "Why not Docker?"<br>Bob: Because `sudo docker`.|[landlock](https://www.kernel.org/doc/html/latest/userspace-api/landlock.html) <br> [seccomp](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)|[lua](http://lua.org/) -> [hlua](hlua) <br> [python](https://python.org/) -> [hpython](hpython) <br> [node](https://nodejs.org/) -> [hnode](hnode) |

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
So given that disclaimer, why did I write this? The secondary goal is of course
to show of Linux's security features in a concrete working setting, but
primarily my goal is for the reader to have added `strace` to her list of
favorite tools.

Alice sends you the `fun.lua` game, hidden (link to code obfuscation
challenges) within is the statement
```lua
os.execute("sudo rm -r /")
```
(or try `sudo --askpass` if not cached).

Lua any effect any where, so hide it in a big lua module
(like a language compiler: link to trust in trust).

```c
#include <sys/prctl.h>
#include <stdlib.h>
void no_new_privs(void)
{
    int r = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
    if(r != 0) {
        abort();
    }
}
```
`no_new_privs` so simple it should always be used, I even have it in my
set of ["c copy-pastas"](https://github.com/rootmos/libr).

But `os.execute("rm -r ~")` would still mess up my day.

`os.execute = nil`, well may be good enough (I haven't figured out a way around
it, but I'm also reasonably sure there may be a clever exploit and would be
interested in seeing it).

This of course can be tweaked into at least making the first naive mitigation
useful:
`os.execute = function() error("not allowed") end`
Especially since the mitigation I would suggest produces a far less
user-friendly error message as we will see.

Enter [seccomp](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html),
which is Linux's way of filtering syscalls and so limiting the exposed surface
of the kernel.
The simplest seccomp filters are essentially accept/reject lists, but can do
more complex things (I haven't looked into it but isn't even cBPF turing complete‽).
Here simple is better since it makes the security model you're implementing
easier to reason about.
So with seccomp enabled with a filter that forbids `clone` (the syscall
that the `exec`-family translates into) the kernel politely kill you, and claim
you received a SIGSYS signal.
My approach have always been to start with a "reject everything" filter,
strace, look for SIGSYS and add that syscall to the accept list if I can
convince myself it won't hurt my desired security level.

```shell
strace lua -e 'os.execute("echo hello")'`
```

When you've done this dance a couple of steps with something you might consider
a normal application you end up with a filter usually including the basic:
`write`, `read`, `close` file descriptor manipulating functions.
`write` is fun to think about: without it how can you communicate the result of
any computation in a "everything is a file" system?
The syscall filtering way of expressing this is seccomp's strict mode: only
allow `write` and `exit`. The reasoning here is that you are only allowed to
`write` to *already opened* filedescriptors, and in this setting `open` is
forbidden.

But even moderately interesting Lua application enjoys using `require`. So it's
not unreasonable to allow Lua to `open` files. But then Alice changes her
`fun.lua` game to include:

```lua
io.open(os.getenv("HOME") .. "/.aws/credentials", "r"):read("*a")
```

(Of course Alice has to get this information back to her, but maybe it's a
multiplayer game? Or she obfuscates it in the game's log file and "oh the game
crashed, why don't you send me the logs?")

Enter [landlock](https://www.kernel.org/doc/html/latest/userspace-api/landlock.html).
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

Lastly I have included a code snippet to drop all capabilities. This is a Linux
feature I previously hadn't had the need to explore (so take that code and what
comes next with a grain of salt and trust, but verify). The classic selling
point of capabilities is the scenario to allow unprivileged users to run
`ping`. In a pre-capabilities world one would have to have to obtain the full
power of the privileged user (read: `root`) in order to use `ping`. (Of course
`setsuid` reduces the mess of every user `su`:ing, but still a nice potential
attack vector.)
The capabilities is basically the idea to split `root` into separate, well,
capabilities that can be granted. (`ping` requires the `CAP_NET_RAW` (?) capability).

In this project this scenario isn't really applicable. But what is applicable
is the function to remove any granted capabilities from the current process.
Maybe this sounds convoluted, but in our current Dockerized world I would say
its fairly common to see images still run as `root`.

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
