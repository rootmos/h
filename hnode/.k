EXE=./hnode

run() {
    make build && "$EXE"
}

trace() {
    make build && strace "$EXE"
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h landlock fail logging util
}
