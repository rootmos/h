INPUT=${INPUT-${K_DIR-.}/test/hello/main.lua}
EXE=$(readlink -f hlua)

run() {
    make LOG_LEVEL=DEBUG && (cd "$(dirname "$INPUT")" && $EXE -st "$(basename "$INPUT")")
}

trace() {
    make LOG_LEVEL=DEBUG && (cd "$(dirname "$INPUT")" && strace $EXE -st "$(basename "$INPUT")")
}

libr() {
    "$HOME/git/libr/bundle" -o r.h landlock fail logging util lua rlimit
}
