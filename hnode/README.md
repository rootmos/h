# hnode

## Usage
```
usage: hnode [OPTION]... INPUT

options:
  -s       allow reading files beneath the input script's directory
  -x       allow executing files beneath the input script's directory (imples read access)
  -h       print this message
  -v       print version information

rlimit options:
  -rRLIMIT=VALUE set RLIMIT to VALUE
  -R             use inherited rlimits instead of default
```

## TODO
- [ ] single-threaded execution (reject `clone` syscall)
