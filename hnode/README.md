# hnode

## Usage
```
usage: hnode [OPTION]... INPUT

options:
  -s       allow reading files beneath the input script's directory
  -h       print this message
  -v       print version information

rlimit options:
  -rRLIMIT=VALUE set RLIMIT to VALUE
  -R             use inherited rlimits instead of default
```

## TODO
- [ ] test using other node versions (at the time of writing only 12 and 19 are
  tested)
- [ ] single-threaded execution (reject `clone` syscall)
