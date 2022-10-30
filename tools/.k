run() {
    make LOG_LEVEL=DEBUG && ./capture ls
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h fail logging util
}
