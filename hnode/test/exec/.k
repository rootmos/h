go() {
    make -C ../.. && pwd && timeout 5s strace -o trace -f -- ../../hnode -R -s main.js
}
