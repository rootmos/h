run() {
    make LOG_LEVEL=DEBUG && ./capture -- sh -c 'echo "hello" 1>&2'
}

libr() {
    "$HOME/git/libr/bundle.sh" -o r.h fail logging util
}
