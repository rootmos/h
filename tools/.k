run() {
    make LOG_LEVEL=DEBUG && ./capture -o stdout -- echo "hello" && cat stdout
}

libr() {
    "$HOME/git/libr/bundle" assemble -o r.h fail logging util
}
