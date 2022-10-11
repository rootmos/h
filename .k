run() {
    make && ./run -s hello.lua
}

trace() {
    make && strace -f ./run -s hello.lua
}
