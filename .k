run() {
    make && ./run hello.lua
}

trace() {
    make && strace -f ./run hello.lua
}
