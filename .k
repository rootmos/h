run() {
    make && ./run -st hello.lua
}

trace() {
    make && strace ./run -st hello.lua
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h landlock fail logging util lua
}
