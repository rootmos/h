run() {
    make && ./run -s hello.lua
}

trace() {
    make && strace ./run -s hello.lua
}
