INPUT=${INPUT-${K_DIR-.}/test/hello/main.py}
EXE=$(readlink -f hpython)

run() {
    make && $EXE "$INPUT"
}

trace() {
    make && strace $EXE "$INPUT"
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h landlock fail logging util
}
