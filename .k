run() {
    make && (cd example && ../hlua -st main.lua)
}

trace() {
    make && (cd example && strace ../hlua -st main.lua)
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h landlock fail logging util lua
}
