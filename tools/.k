run() {
    make LOG_LEVEL=DEBUG && ./capture sleep 5
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h fail logging util
}
