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
The secondary goal is of course to show of Linux's security features in a
concrete setting, but my primary my goal is for the reader to have added
`strace` to her list of favorite tools.

Assume Alice is a game designer with malicious intents and you are her intended
victim.
Being a fan of indie games you of course accept to be a beta-tester for her
latest creation.
So Alice sends you the `fun.lua` game, hidden within is the statement
```lua
os.execute("sudo rm -r /")
```
(or maybe she'll try `sudo --askpass` if the credentials aren't cached).
Of course a diligent code-reviewer might catch such an obviously malicious
statement.
But such a statement can be surprisingly easy to miss in a hurried glance:
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
Meaning that it's very feasible to bundle a compiler for another domain or
other esoteric language (such as [fennel](https://fennel-lang.org) and
[Amulet](https://amulet.works/).
But Lua (as well as python, node, c and many many more) are:
any-effect-any-time languages.
This in contrast with [Haskell](https://www.haskell.org)
(check out [Learn You a Haskell for Great Good!](http://www.learnyouahaskell.com))
or [eff](https://www.eff-lang.org/) if you're feeling adventurous.
That means that an expected pure/side-effect free operation such as compiling a
piece of source code can execute the above `os.execute`-attack or worse if the
attacker has an even more insidious mind.
And considering that a compilers are usually quite extensive pieces of software
they provide ample forestry to hide the malicious tree.
(I suggest splitting the malicious code in several commits/PR:s.)
Here I highly recommends [Ken Thompson's "Reflections on Trusting Trust"](https://dl.acm.org/doi/10.1145/358198.358210),
which if you haven't read I expect will shatter any trust you might have
imagined you had in *any* piece of software.

So the world is a scary and unsatisfactory environment, then let's begin
mitigating the consequences.

Alice's `sudo` based `rm -f /`-attack can be mitigated by a one-liner:
[`prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)`](https://man.archlinux.org/man/prctl.2#PR_SET_NO_NEW_PRIVS).
This call is not expected to fail, but being a conscientious developer it never
hurts to "crash-don't-thrash" and presented as a copy-paste-able snippet:
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
Of course you might prefer `exit`.
I don't, because libc:s commonly provide
[`atexit`](https://man.archlinux.org/man/atexit.3)
which in my opinion is contrary to a fail-early/crash-don't-thrash philosophy:
the operating system already have to assume the responsibility to clean up
after a failing process.
(Ever noticed that c coders don't free their allocations when exiting?)
A side-note: consider other programming models such as the actor model
where the non-delivery of a message is a scenario brought to the forefront.
If you are curious I warmly recommend [Erlang](https://www.erlang.org/)
(checkout [Learn You Some Erlang for great good!](https://learnyousomeerlang.com/)).

Back to the mitigations: the above `no_new_privs` function call is so simple
it should always be used.
Unless *explicitly necessary* to gain new privileges.
This is the [Principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege):
if the functionality you intend to provide do not require privileges your
process should not have any privileges, and this is the red thread in this
attempt at raison d'être.
However, normally started processes inherit quite a handful of privileges that
Alice can still abuse, as we shall see.

So, Alice can't `sudo` anymore thanks to `PR_SET_NO_NEW_PRIVS`.
But even a sneaky `os.execute("rm -r ~")` would still majorly mess up my day,
(and make me unfriend Alice).

The naive Lua specific mitigation is to `os.execute = nil` before running the
entrypoint of Alice's game.
Well, that may be good enough.
(I haven't figured out a way around that mitigation, but I'm reasonably sure
there may be a clever exploit and would be interested in seeing it.)
Continuing this idea we can weak it into at least making this first naive
mitigation useful:
```lua
os.execute = function() error("not allowed") end
```
Especially since the mitigation I suggest produces a far less
user-friendly error message, as we will see.

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
