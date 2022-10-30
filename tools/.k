run() {
    make LOG_LEVEL=DEBUG && ./capture echo "hello"
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h fail logging util
}
