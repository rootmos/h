INPUT=${INPUT-${K_DIR-.}/test/require}
TEST_RUNNER=$K_DIR/../tools/test-runner

export TIMEOUT=5

run() {
    make build LOG_LEVEL=DEBUG && $TEST_RUNNER "$INPUT"
}

trace() {
    make build LOG_LEVEL=DEBUG && $TEST_RUNNER "$INPUT" --trace
}

libr() {
    "$HOME/git/libr/bundle" -o r.h landlock fail logging util uv rlimit
}
