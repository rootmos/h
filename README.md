# *h*ardened script host programs
[![Build and test](https://github.com/rootmos/h/actions/workflows/build-test.yaml/badge.svg)](https://github.com/rootmos/h/actions/workflows/build-test.yaml)

Unprivileged sandboxing of popular script languages.

|Unprivileged|sandboxing|script languages|
|------------|----------|----------------|
|[Alice: "Why not Docker?"<br>Bob: "Because `sudo docker`".](#frequently-unasked-questions)|[landlock](https://www.kernel.org/doc/html/latest/userspace-api/landlock.html) <br> [seccomp](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)|[lua](http://lua.org/) -> [hlua](hlua) <br> [python](https://python.org/) -> [hpython](hpython) <br> [node](https://nodejs.org/) -> [hnode](hnode) <br> [bash](https://www.gnu.org/software/bash/) -> [hsh](hsh)|

## DISCLAIMER
This project is a work in progress and has not been audited by security
experts.

However I think it remains useful for educational purposes regarding Linux's
sometimes daunting security features and using `strace` to illustrate how a
program written in a high level language is translated into syscalls to obtain
its desired (or undesired) effects.

... and it is certainly better than nothing as I will try to exemplify in the
following section. But as always, remember that sandboxing and containerization
only limit the extent of a successful attack,
and don't give you carte blanche to willy-nilly execute untrusted code.

## Raison d'être
So given that disclaimer, why did I write this?
Showcasing Linux's security features is only a secondary goal; my primary goal
is for the reader to add `strace` to her list of favorite tools.

### Alice's [game](https://love2d.org/)
Assume Alice is a game designer with malicious intent and you are her intended
victim.
Being a fan of indie games you of course accept to be a beta-tester for her
latest creation.
She sends you the `fun.lua` game and hidden within is the statement:
```lua
os.execute("sudo rm -rf --no-preserve-root /")
```
(or she'll try `sudo --askpass` if the credentials aren't cached).
A diligent code-reviewer might catch such an obviously malicious
statement, but it can be surprisingly easy to miss in a hurried
glance; try to allow yourself only a few seconds to read the following:
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
Did you spot the malicious or unintended transposition?
This is the hardship presented to us by the PR-culture, and can provide a false
sense of security.
There are also programming languages
[designed to be difficult to read](https://esolangs.org/wiki/Esoteric_programming_language#Obfuscation).
And speaking of programming languages:
"the greatest thing about Lua is that you don't have to write Lua."
Meaning that it's very feasible to bundle a compiler for another language,
however non-esoteric (check out: [fennel](https://fennel-lang.org) and
[Amulet](https://amulet.works/)).
But Lua (as well as Python, Node.js, C and many many more) are:
[any-effect-at-any-time languages](https://www.youtube.com/watch?v=iSmkqocn0oQ).
This in contrast with [Haskell](https://www.haskell.org)
(check out [Learn You a Haskell for Great Good!](http://www.learnyouahaskell.com))
or maybe [eff](https://www.eff-lang.org/) if you're feeling adventurous.
That means that an expected pure/side-effect free operation such as compiling a
piece of source code can include an obfuscated `os.execute`-attack or worse
if the attacker has a more insidious mind.

Considering that compilers are usually quite extensive pieces of software
they provide ample forestry to hide a malicious tree.
Alice, I suggest you split your malicious code into several commits and PR:s
(preferably large ones close to a deadline).
For the victim, I recommend [Ken Thompson's "Reflections on Trusting Trust"](https://dl.acm.org/doi/10.1145/358198.358210),
which if you haven't read I expect will shatter any trust you might have
imagined you had in *any* binary executable
(going all the way back to punchcards and the [PDP-1](https://en.wikipedia.org/wiki/PDP-1)).
This may seem ridiculous, but [OCaml](https://ocaml.org/)
(my yardstick language of languages):
still bundle a [bootstrapping binary complier](https://github.com/ocaml/ocaml/blob/trunk/boot/ocamlc)
to build subsequent compilers: this is very much
["tusting trust"](https://dl.acm.org/doi/10.1145/358198.358210).
Even more so since [`Coq`](https://en.wikipedia.org/wiki/Coq) is implemented
in and thus compiled by OCaml; now your trust stack ends in a binary blob:
do you trust it? And do you have the incredibly time consuming task of
verifying no malicious opcodes hide within?

So the world is a scary and unsatisfactory environment, let's consider
mitigating the consequences of malicious and/or incompetently written
code.

### Enter [no new privileges](https://www.kernel.org/doc/html/latest/userspace-api/no_new_privs.html)
Alice's `sudo`-based `rm`-attack can be mitigated by a one-liner:
[`prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)`](https://man.archlinux.org/man/prctl.2#PR_SET_NO_NEW_PRIVS).
This call is not expected to fail, but being a conscientious developer it never
hurts to crash-don't-thrash and I present a
[copy-pastable snippet](https://github.com/rootmos/libr/blob/master/modules/no_new_privs.c):
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
You might prefer [`exit`](https://man.archlinux.org/man/exit.3a).
I don't: [libc](https://libc.org):s commonly provide
[`atexit`](https://man.archlinux.org/man/atexit.3)
which in my opinion is contrary to a fail-early/crash-don't-thrash philosophy:
the operating system already has to assume the responsibility of clean up
after a failing process.
(Ever noticed that C coders don't free their allocations when exiting?)
Using `exit` and `atexit` reminds me of languages with exceptions and the
nightmare when exception handlers raise exceptions ad infinitum.
Instead consider programming models where failure-is-always-an-option thinking
is prevalent, such as the actor model:
where the non-delivery of a message is a scenario
brought to the forefront (with the real-world scenario of the fallibility of
network connections).
If you are curious I recommend [Erlang](https://www.erlang.org/)
(check out [Learn You Some Erlang for great good!](https://learnyousomeerlang.com/)).
(Erlang does not implement the Actor model in a strict way, but provide a very
enjoyable way to explore its concepts while writing highly concurrent
applications.)

Back to mitigating Alice's attacks: the above
[`no_new_privs`](https://github.com/rootmos/libr/blob/master/modules/no_new_privs.c)
call is so simple it should always, *always*, be used:
unless *explicitly necessary* to gain new privileges.
This is the [Principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege):
if the functionality you intend to provide does not require privileges your
process should not have any privileges, and this is the red thread in this
attempted raison d'être.
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
there is an exploit and would be interested in seeing it).
Continuing this idea we can tweak it into at least making this first naive
mitigation useful:
```lua
os.execute = function() error("not allowed") end
```
Especially since the mitigation I suggest below do not even allow for the
program to try to provide a user-friendly error message.

### Enter [seccomp](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html)
Seccomp is Linux's way of filtering syscalls and so limiting the exposed
kernel surface.

Fancy words aside, this means that when you receive in your email inbox the
notification of a new vulnerability you can feel certain that you are not
affected because the vulnerable syscalls are rejected by your program.
If you aren't subscribed to any [CVE](https://en.wikipedia.org/wiki/Common_Vulnerabilities_and_Exposures)
mailing lists I recommend:
- [Arch Linux's](https://lists.archlinux.org/mailman3/lists/arch-security.lists.archlinux.org/),
- [Ubuntu's](https://lists.ubuntu.com/mailman/listinfo/ubuntu-security-announce) or
- [OpenBSD's](https://www.openbsd.org/mail.html) mailing lists.

The simplest seccomp filters are essentially accept/reject lists, but they can
do more complex things.
But as always when it comes to security:
easily understandable code is always preferred.

Back to Alice's `os.execute`-based attacks:
with seccomp enabled with a filter that forbids `exec`:s,
the kernel will politely kill your process and suggest to the rest of the
system that you received a `SIGSYS` signal.
In practice this means that your process immediately vanishes, so without a
syscall inspection tool such as `strace` one is reduced to debugging by:
thou shalt `printf("are we nearly here yet?");`

### Enter [strace](https://man.archlinux.org/man/strace.1)
If you haven't invoked strace before, or you are curious what syscalls are
being used by a program, then try:
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
Note the `-f` (`--follow-forks`) option that tells `strace` to continue tracing
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
Remember, in Linux threads and processes are the same abstraction:
essentially one with shared virtual memory space and the other without
(but even a casual glance at clone's options makes the difference no longer so
clear cut).
Now with `clone` rejected: both threads as well as processes are no longer
things you need to reason about.

So how do we actually tell seccomp what to accept and what to reject?

### Enter [Berkeley Packet Filters](https://www.kernel.org/doc/html/latest/bpf/index.html)
Seccomp filters are expected to be binary representations of
[cBPF](https://www.kernel.org/doc/Documentation/networking/filter.txt), the c
stands for "classic" BPF (in contrast with
extended BPF ([eBPF](https://www.kernel.org/doc/html/latest/bpf/index.html)).

While cBPF is not theoretically Turing complete
because of lack of infinite memory; it is restricted to the scratch memory:
`uint32_t M[16]`.
That only presents an interesting challenge:
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
syscall, reluctantly add it to the allowed list and iterate:
```
ld [$$offsetof(struct seccomp_data, nr)$$]

jeq #$__NR_brk, good

bad: ret #$SECCOMP_RET_KILL_THREAD
good: ret #$SECCOMP_RET_ALLOW
```
Eventually when the test passes you have achieved a list of syscalls
living up to the [principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege).

The filter you produce might appear very long, but remember that Linux has a
[*massive* amount of syscalls](https://git.musl-libc.org/cgit/musl/tree/arch/x86_64/bits/syscall.h.in?h=v1.2.3&id=7a43f6fea9081bdd53d8a11cef9e9fab0348c53d).
Viewed from a security perspective even a moderately long filter is still
a huge reduction of the exposed kernel surface.
But a simple (but still very effective) yes/no approach to filtering
syscalls falls short when it encounters the
"functionality-grouping" syscalls such as
[`fcntl`](https://man.archlinux.org/man/fcntl.2),
[`ioctl`](https://man.archlinux.org/man/ioctl.2) and
[`prctl`](https://man.archlinux.org/man/prctl.2)
(which [we encountered above](#enter-no-new-privileges)).
For these syscalls it becomes necessary to inspect the call arguments
(from [`hlua`'s filter](hlua/filter.bpf)):
```
jne #$__NR_fcntl, fcntl_end
ld [$$offsetof(struct seccomp_data, args[1])$$]
jeq #$F_GETFL, good
jmp bad
fcntl_end:
```

Sometimes it may be useful to "tamper" with a syscall instead of rejecting it
outright:
return `-1` and set `errno` to `EPERM` or `ENOSYS` to allow a child to recover:
see for example the `prlimit` check in
[`hnode`'s seccomp filter](https://github.com/rootmos/h/blob/6ed41b19839291fe4ca404cb5c315223a0f72ec2/hnode/filter.bpf#L76).

Doing the "test-n-strace" dance for a non-trivial test-case you quickly end up
with a filter usually including the
`read`, `write` and `close` syscalls.
(Unsurprisingly these have syscall numbers:
[`0`, `1` and `3`](https://git.musl-libc.org/cgit/musl/tree/arch/x86_64/bits/syscall.h.in?h=v1.2.3&id=7a43f6fea9081bdd53d8a11cef9e9fab0348c53d#n1).)

`write` is particularly fun to think about: without it
[how can you communicate the result of any computation](https://youtu.be/iSmkqocn0oQ?t=195)
in a "everything is a file" system?
The syscall filtering way of expressing this is
[seccomp's strict mode](https://man.archlinux.org/man/seccomp.2#SECCOMP_SET_MODE_STRICT):
only allow `write` and `exit`. The reasoning being is that you are only allowed
to `write` to *already opened* file descriptors (since in this setting
`open` is forbidden, or more accurately not expressively allowed).

But even moderately interesting Lua applications enjoy using
[`require`](https://www.lua.org/manual/5.4/manual.html#pdf-require).
So it's not unreasonable to allow Lua to `open` files (which fills in the
number `2` syscall numbering slot).
But then Alice changes her `fun.lua` game to include (obfuscated of course):
```lua
io.open(os.getenv("HOME") .. "/.aws/credentials", "r"):read("*a")
```
Now Alice has to get this information back to her, but maybe it's a
multiplayer game? Or she obfuscates it in the game's log file and exclaims:
"Oh the game crashed, why don't you send me the logs?"

Alice's intentions might only go as far as
[griefing](https://en.wikipedia.org/wiki/Griefing), and she will try to
`os.remove` your access tokens.
Alice, try removing Chrome/Firefox cookies as well.
This would definitely lose me my sunny disposition.

Removing files map to the [`unlink`](https://man.archlinux.org/man/unlink.2)
syscall.
Certainly it commonly makes sense to reject it,
but a plausible scenario is using `unlink` is to remove intermediately
created files (during compilation maybe).

So what do we do about Alice's intent to remove your
`with-blood-sweat-and-tears.doc` file?

### Enter [landlock](https://www.kernel.org/doc/html/latest/userspace-api/landlock.html)
Landlock is a fairly recently added security feature, which is meant to
restrict filesystem access for unprivileged processes, in addition to the
standard UNIX file permissions.
(I will argue landlock is fairly recent when its new syscalls have, at the time
of writing: [the highest syscall number](https://git.musl-libc.org/cgit/musl/tree/arch/x86_64/bits/syscall.h.in?h=v1.2.3&id=7a43f6fea9081bdd53d8a11cef9e9fab0348c53d#n355).)

In essence [landlock](https://man.archlinux.org/man/landlock.7)
grants or restricts rights to filesystem operations
on whole filesystem hierarchies. (Note that a single file is a trivial
hierarchy.)
So we can grant read access to `/usr/lib` only and mitigate Alice's attack on
your access tokens in your home directory. And maybe allow both read and write
to `/tmp`, and maybe allow removing (i.e. unlinking).
Unless you allow `open`'s
[`O_TMPFILE` flag](https://man.archlinux.org/man/open.2#O_TMPFILE)
in your seccomp filter of course.

The reason this section is bare of example code is that I found, and hope you
will do too, the concepts behind landlock easily understandable and yet very
powerful.
Therefore I will not include any sample code here since the
[the sample code provided with landlock](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/samples/landlock/sandboxer.c?h=v6.0.7&id=3a2fa3c01fc7c2183eb3278bd912e5bcec20eb2a)
is excellent, and is relatively
[verbatim what I use](https://github.com/rootmos/libr/blob/master/modules/landlock.c).
My experienced ratio of positive security impact versus time spent learning
the feature is huge.

My one criticism of the current implementation of landlock is the inability
to hide files: that is, even though landlock restricts access to a
file, for example `/etc/passwd`, then `stat` (or similar) responds with
`EACCESS` instead of `ENOENT`.
The knowledge that a Linux installation has a `/etc/passwd` maybe of limited
value, but revealing that `~/.aws/credentials` exist
can enable an attacker to target her attack more effectively against the
discovered files.

Furthermore an attacker can, given enough access to
(lots, lots and lots of)
system time, enumerate your entire file tree.
This is of course a ridiculous endeavour;
[`#define NAME_MAX 255`](https://git.musl-libc.org/cgit/musl/tree/include/limits.h?h=v1.2.3&id=7a43f6fea9081bdd53d8a11cef9e9fab0348c53d#n45)
and [`pp`](tools/pp) `('z'-'a')+('Z'-'A')+('9'-'0')` says that the amount of
filenames to try are *bounded from below* by
[`59^255`](https://www.wolframalpha.com/input?i=59%5E255):
which evaluates to an integer that starts with `36920` and ends with `89299`
(not mentioning the other 442 digits).

The counter-argument is that there are other, perhaps better, ways of achieving
this functionality
([`chroot`](https://man.archlinux.org/man/chroot.2.en) maybe),
reducing my criticism to a mere down-prioritized item on a wishlist.

The wrinkle in our concrete setting of providing script hosts is that sometimes
the interpreters want to dynamically load shared libraries which boast a
notorious elusiveness and never appear in the same place twice
([which implies we at least know their velocity](https://en.wikipedia.org/wiki/Uncertainty_principle)).
Hence I have added a set of tools to, at compile time,
tell [`hpython`](hpython)'s landlock rules to allow read access to the path
where the embedded Python instance will look for, say, the `libz` library.
This is the functionality exercised in
[`hpython`'s `import` test](hpython/test/import).
The journey starts with the [`paths`](tools/paths) utility:
```shell
paths --python-site -lz
```
which for my system suggest that these file system trees are of
particular interest:
```
/usr/lib/python3.10
/usr/lib/libz.so.1
```
Behind the scenes [`dlinfo`](https://man.archlinux.org/man/dlinfo.3) is used to
resolve the shared libraries.
The paths are then
[inspected and converted into a relevant landlock rules code snippet](tools/landlockc)
which is then included and applied in the main program.

Continuing the same wrinkle into the [`hsh`](hsh) project
which [executes a `bash`](https://man.archlinux.org/man/core/man-pages/fexecve.3.en)
in the same security setting as the other script hosts programs
produce another complication.
Being a fully-fledged shell it desires to link quite a bit
of dynamic libraries, which in turn desire even more of that shared binary
goodness.
[`ldd /bin/bash`](https://man.archlinux.org/man/ldd.1) expose the extent of
their desire:
```
     linux-vdso.so.1 (0x00007ffe3bed2000)
     libreadline.so.8 => /usr/lib/libreadline.so.8 (0x00007f45e8bcc000)
     libdl.so.2 => /usr/lib/libdl.so.2 (0x00007f45e8bc7000)
     libc.so.6 => /usr/lib/libc.so.6 (0x00007f45e89e0000)
     libncursesw.so.6 => /usr/lib/libncursesw.so.6 (0x00007f45e896c000)
     /lib64/ld-linux-x86-64.so.2 => /usr/lib64/ld-linux-x86-64.so.2 (0x00007f45e8d44000)
```
That indeed is a wrinkle to iron out given the
[diversity of Linux distributions](https://en.wikipedia.org/wiki/Linux_distribution#/media/File:Linux_Distribution_Timeline_21_10_2021.svg).
My [`awk`-front-leds and `sed`-mid-legs twitch](https://en.wikipedia.org/wiki/The_Metamorphosis)
but there is a better approach using [`objdump`](https://man.archlinux.org/man/ldd.1#Security):
```shell
objdump -p /path/to/program | grep NEEDED
```
which said insect has bundled into the [`poor_ldd`](tools/poor_ldd) utility
(using the above mentioned [`dlinfo`](https://man.archlinux.org/man/dlinfo.3)
based [`lib`](tools/lib.c) utility).
Now `poor_ldd /bin/bash` produce a similar output as `ldd`:
```
/lib64/ld-linux-x86-64.so.2
/usr/lib/libc.so.6
/usr/lib/libdl.so.2
/usr/lib/libncursesw.so.6
/usr/lib/libreadline.so.8
```
which then can be handed of to [landlockc](tools/landlockc) and grant a very
limited set of read-access rules.

Alice's set attack vectors are now quite diminished, but we can do even better.

### Enter [drop capabilities](https://man.archlinux.org/man/capabilities.7)
I have included a [code snippet](build/capabilities.c) to drop capabilities.
This is a Linux feature I previously hadn't had the need to explore (so take
that code and what comes next with a grain of salt and always:
"trust, but verify").
The classic selling point of capabilities is the scenario to allow unprivileged
users to run `ping`.
In a pre-capabilities world one would have to have to obtain the full
power of the privileged user (`root`) in order to use `ping`.
Of course `setsuid` reduces the mess of every user `su`:ing, but still
provides a nice potential attack vector on the `ping` binary.
The capabilities is basically the idea to split `root` into separate, well,
capabilities that can be granted independently.
(`ping` requires the [`CAP_NET_RAW`](https://man.archlinux.org/man/capabilities.7.en#CAP_NET_RAW)
capability).

In this project this scenario isn't really applicable
(since we start out as unprivileged users.)
But what may be applicable is the functionality to relinquish granted
capabilities from the current process.
Maybe this sounds convoluted, but in our current Dockerized world I would say
its fairly common to see images invoke executables in a privileged mode
(i.e. [not setting another user](https://docs.docker.com/engine/reference/builder/#user)).

And a noteworthy configuration option of Linux is that you don't have to include
the bothersome userland. Here I imagine a barebones server setup: the kernel,
a single stand-alone server executable (serving as the
[init](https://man.archlinux.org/man/init.1) process) and nothing else.
In that setting dropping capabilities could be useful.

But even with these restrictions Alice can cause quite a bother:

### Enter [rlimits](https://man.archlinux.org/man/core/man-pages/setrlimit.2.en)
Now Alice is restricted to using a pre-approved set of syscalls
and restricted to a pre-approved set of file-system operations on an
equally pre-approved subset of the file-system tree.

Her last-ditch effort is to execute a
[Denial-of-service attack](https://en.wikipedia.org/wiki/Denial-of-service_attack).
I suggest Alice tries to `while(1)` allocate at least one page of memory
(`getconf PAGESIZE`: 4096 bytes),
write a single [pseudo-randomly generated](https://en.wikipedia.org/wiki/Xorshift)
byte to each allocation: forcing the kernel to
[copy-on-write](https://en.wikipedia.org/wiki/Copy-on-write).
This will quickly exhaust all available memory, and any unfortunate Linux user
will attest to the ensuing misery.

The mitigation is to apply strict
[rlimits](https://man.archlinux.org/man/core/man-pages/setrlimit.2.en).
In this attack [`RLIMIT_AS`](https://man.archlinux.org/man/core/man-pages/setrlimit.2.en#RLIMIT_AS)
might be the most efficient mitigation.
The common way of applying `rlimits` is by using the shell's
[`ulimit`](https://man.archlinux.org/man/ulimit.1p) command.

Alice then tries a [fork bomb](https://en.wikipedia.org/wiki/Fork_bomb).
Rejecting the `clone` syscall will of course mitigate such an attack, but for
instance: [`node`](hnode) is determined to spawn worker threads making such a
mitigation ineffective.
Once more rlimits come to the rescue:
[`RLIMIT_NPROC`](https://man.archlinux.org/man/core/man-pages/setrlimit.2.en#RLIMIT_NPROC)
restricts the number of process a process can spawn
(including threads of course).

Alice, your next attack vector should be to exhaust any available block-devices
by creating huge files with your pseduo-random generator.
But again rlimits provides the mitigation:
[`RLIMIT_FSIZE`](https://man.archlinux.org/man/core/man-pages/setrlimit.2.en#RLIMIT_FSIZE).

The pattern should be obvious: restrict all available `rlimits` to the minimum
required to make the intended functionality succeed.
The [code-snippet used](https://github.com/rootmos/libr/blob/master/modules/rlimit.c)
to restrict the `rlimits` zeroes any resource restriction not expressively
required non-zero limit.
Check the `#define RLIMIT_DEFAULT_`:s at the top of [hlua](hlua/main.c),
[hpython](hpython/hpython.c) and [hnode](hnode/main.cpp).
Again we encounter the [principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege).

For instance, this approach guarantees that a file-descriptor-exhaustion
attack is no longer viable:
[`RLIMIT_NOFILE`](https://man.archlinux.org/man/core/man-pages/setrlimit.2.en#RLIMIT_NOFILE).

### Conclusion
So given Alice's game you're itching to play regardless of her malicious intent:
do you now feel safe enough to evaluate her code?
- We can enforce a list of allowed syscalls and their arguments using
  [seccomp](#enter-berkeley-packet-filters)
- We can impose an additional layer of access restriction upon the file
  system hierarchy using [landlock](#enter-landlock)
- We can enforce [strict resource usage limits](#enter-rlimits) on:
  memory usage, file-descriptor and thread/processes allocation

You might feel safe enough: but what
[surreal thing will she think of next](https://cs.stanford.edu/~knuth/sn.html)‽

## Frequently unasked questions
- "No, but seriously, why not `sudo docker`?"<br>
  Yes, and seriously, no `sudo`, but yes Docker. My opinion is that Docker is
  great (for me `Docker := cgroups+overlayfs` packaged into a sleek product,
  but that's fine), especially in professional CI/Kubernetes/what-have-you
  settings. With this project I want to showcase a Linux way of sandboxing
  applications *unprivileged*: hence no `sudo`, but yes Docker.
  Also my guiding principle (other than "trust, but verify" that is);
  [principle of least privilege](https://en.wikipedia.org/wiki/Principle_of_least_privilege):
  why require privileges to do something that can be achieved without?
- "Why no binary packages?"<br>
  Because each embedded script host may have a different license, and I do not
  want to spend the time to study each of them and mess up anyway.
  Also my aim for this project to be an educational showcase and a sandbox to
  let users experiment and get hands on experience with the Linux security
  features in a non-toy setting: reducing the value of a pre-built binary
  package distributed without the sources and tools.
  The laziness argument coupled with this argument guides me to only offer
  source packages: mostly as a guide for users who do not feel comfortable to
  jump into the deep end with `git clone` and `make`.

## Installation and build instructions

### Building from sources
This project is intended to be built using `Makefile`:s and common C build
tools, except for the `bpf_asm` tool (found in the
[Linux kernel sources](https://github.com/torvalds/linux/tree/master/tools/bpf)).
Arch Linux users can use the [`bpf`](https://archlinux.org/packages/community/x86_64/bpf/)
package, but other distributions might have to build their own copies.
I have prepared a [build script](tools/bpf/build) which is used when `bpf_asm`
is not found (the script is used in the Ubuntu workflow job).

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
|Ubuntu 24.04| [`libcap2`](https://packages.ubuntu.com/noble/libcap2) [`lua5.4`](https://packages.ubuntu.com/noble/lua5.4) [`python3`](https://packages.ubuntu.com/noble/python3) [`libnode109`](https://packages.ubuntu.com/noble/libnode109) [`bash`](https://packages.ubuntu.com/noble/bash) | [`make`](https://packages.ubuntu.com/noble/make) [`pkg-config`](https://packages.ubuntu.com/noble/pkg-config) [`gcc`](https://packages.ubuntu.com/noble/gcc) [`libcap-dev`](https://packages.ubuntu.com/noble/libcap-dev) [`wget`](https://packages.ubuntu.com/noble/wget) [`ca-certificates`](https://packages.ubuntu.com/noble/ca-certificates) [`bison`](https://packages.ubuntu.com/noble/bison) [`flex`](https://packages.ubuntu.com/noble/flex) [`liblua5.4-dev`](https://packages.ubuntu.com/noble/liblua5.4-dev) [`python3`](https://packages.ubuntu.com/noble/python3) [`libpython3-dev`](https://packages.ubuntu.com/noble/libpython3-dev) [`libnode-dev`](https://packages.ubuntu.com/noble/libnode-dev) |  |
|Ubuntu 22.04| [`libcap2`](https://packages.ubuntu.com/jammy/libcap2) [`lua5.4`](https://packages.ubuntu.com/jammy/lua5.4) [`python3`](https://packages.ubuntu.com/jammy/python3) [`libnode72`](https://packages.ubuntu.com/jammy/libnode72) [`bash`](https://packages.ubuntu.com/jammy/bash) | [`make`](https://packages.ubuntu.com/jammy/make) [`pkg-config`](https://packages.ubuntu.com/jammy/pkg-config) [`gcc`](https://packages.ubuntu.com/jammy/gcc) [`libcap-dev`](https://packages.ubuntu.com/jammy/libcap-dev) [`wget`](https://packages.ubuntu.com/jammy/wget) [`ca-certificates`](https://packages.ubuntu.com/jammy/ca-certificates) [`bison`](https://packages.ubuntu.com/jammy/bison) [`flex`](https://packages.ubuntu.com/jammy/flex) [`liblua5.4-dev`](https://packages.ubuntu.com/jammy/liblua5.4-dev) [`python3`](https://packages.ubuntu.com/jammy/python3) [`libpython3-dev`](https://packages.ubuntu.com/jammy/libpython3-dev) [`libnode-dev`](https://packages.ubuntu.com/jammy/libnode-dev) | [`python3-toml`](https://packages.ubuntu.com/jammy/python3-toml) |
|Arch Linux| [`lua`](https://archlinux.org/packages/extra/x86_64/lua/) [`python`](https://archlinux.org/packages/core/x86_64/python/) [`nodejs`](https://archlinux.org/packages/extra/x86_64/nodejs/) [`bash`](https://archlinux.org/packages/core/x86_64/bash/) | [`bpf`](https://archlinux.org/packages/extra/x86_64/bpf/) |  |

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
- [ ] use [`strace` statistics](https://man.archlinux.org/man/strace.1#Statistics)
  to sort seccomp filters with respect to number of calls
- [ ] landlock ABI=2 (see the [sandbox example](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/samples/landlock/sandboxer.c#n233))
- [ ] [readline](https://en.wikipedia.org/wiki/GNU_Readline) or naive
  [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop)
  with [rlwrap](https://github.com/hanslub42/rlwrap)
- [ ] reference [OpenBSD's pledge(2)](https://man.openbsd.org/pledge.2)
