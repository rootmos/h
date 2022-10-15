# *h*ardened script host programs

[![Test](https://github.com/rootmos/h/actions/workflows/test.yaml/badge.svg)](https://github.com/rootmos/h/actions/workflows/test.yaml)

Unprivileged sandboxing of popular script languages.

## Unprivileged
Alice: "Why not Docker/cgroups?"

Bob: "Because `sudo docker`."

## sandboxing
* [landlock](https://www.kernel.org/doc/html/latest/userspace-api/landlock.html) to limit filesystem access
* [seccomp](https://www.kernel.org/doc/html/latest/userspace-api/seccomp_filter.html) to filter syscalls

## of popular script languages.
* [lua](http://lua.org/) -> [hlua](hlua)
* [python](https://python.org/) -> [hpython](hpython)
