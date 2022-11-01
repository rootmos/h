run() {
    make LOG_LEVEL=DEBUG && ./capture -o stdout -- echo "hello" && cat stdout
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h fail logging util
}
