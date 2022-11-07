INPUT=${INPUT-${K_DIR-.}/test/socket}
TEST_RUNNER=$K_DIR/../tools/test-runner

run() {
    make build && $TEST_RUNNER "$INPUT" | jq -r .
}

trace() {
    make build && $TEST_RUNNER "$INPUT" --trace | jq -r .trace
}

libr() {
    "$HOME/git/libr/bundle" -o r.h fail logging \
        no_new_privs seccomp landlock rlimit
}
