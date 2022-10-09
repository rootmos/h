ld [$$offsetof(struct seccomp_data, arch)$$]
jne #$AUDIT_ARCH_X86_64, bad
ld [$$offsetof(struct seccomp_data, nr)$$]
jeq #$__NR_rt_sigreturn, good
jeq #$__NR_exit_group, good
jeq #$__NR_exit, good
jeq #$__NR_read, good
jeq #$__NR_write, good
jeq #$__NR_fstat, good
jeq #$__NR_mmap, good
jeq #$__NR_rt_sigprocmask, good
jeq #$__NR_rt_sigaction, good
jeq #$__NR_nanosleep, good
bad: ret #$SECCOMP_RET_KILL_THREAD
good: ret #$SECCOMP_RET_ALLOW
