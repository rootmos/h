EXE=./hnode

run() {
    make build && "$EXE" 'console.log("hello world")'
}

trace() {
    make build && strace "$EXE" 'console.log("hello world")'
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h landlock fail logging util
}
