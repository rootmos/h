run() {
    make && ./landlock
}

trace() {
    make && strace -f ./landlock
}
