INPUT=${INPUT-${K_DIR-.}/test/hello/main.lua}
EXE=$(readlink -f hlua)

run() {
    make && (cd "$(dirname "$INPUT")" && $EXE -st "$(basename "$INPUT")")
}

trace() {
    make && (cd "$(dirname "$INPUT")" && strace $EXE -st "$(basename "$INPUT")")
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h landlock fail logging util lua
}
