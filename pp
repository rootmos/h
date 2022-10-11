#!/bin/bash

set -o nounset -o pipefail -o errexit

INCLUDE=()
INCLUDE+=("fcntl.h")
INCLUDE+=("linux/unistd.h")
INCLUDE+=("linux/seccomp.h" "linux/audit.h")

FMT=%d
NEWLINE='\n'
while getopts "nxud-" OPT; do
    case $OPT in
        n) NEWLINE= ;;
        x) FMT=0x%x ;;
        u) FMT=%u ;;
        d) FMT=%d ;;
        -) break ;;
        ?) exit 2 ;;
    esac
done
shift $((OPTIND-1))

src() {
    for i in "${INCLUDE[@]}"; do
        echo "#include <$i>"
    done
    cat <<EOF
#include <stdio.h>
int main() { return printf("$FMT$NEWLINE", $1), 0; }
EOF
}

expression() {
    if command -v tcc > /dev/null; then
        src "$1" | tcc -run -
    else
        TMP=$(mktemp -d)
        trap 'rm -rf $TMP' EXIT

        src "$1" > "$TMP/a.c"
        cc -o "$TMP/a" "$TMP/a.c"
        "$TMP/a"
    fi
}

if [ "${1--}" == "-" ] || [ -f "$1" ]; then
    cat "${1--}" | lua <(cat <<EOF
local function f(s)
    return io.popen("$0 -nx '" .. s .. "'"):read("*a")
end
local bs = io.read("*a"):gsub("%$%\$([^$]+)%$%$",f):gsub("%\$([a-zA-Z0-9_]+)",f)
io.write(bs)
EOF
)
else
    expression "$1"
fi
