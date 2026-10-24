#!/bin/sh
mode="$1"
shift
if [ -z "$mode" ] || [ $# -eq 0 ]; then
    echo "usage: golden.sh check|update command [args...]" >&2
    exit 2
fi

root=$(cd "$(dirname "$0")/.." && pwd)
golden="$root/tests/golden"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
pass=0
fail=0

capture() {
    dir="$1"
    file="$2"
    out="$3"
    shift 3
    (
        cd "$dir" || exit 2
        "$@" "$file" > "$work/stdout" 2> "$work/stderr"
        echo "$?" > "$work/status"
    )
    {
        cat "$work/stdout"
        echo "--- stderr ---"
        cat "$work/stderr"
        echo "--- exit: $(cat "$work/status") ---"
    } > "$out"
}

verify() {
    name="$1"
    actual="$2"
    expected="$3"
    if [ "$mode" = "update" ]; then
        mkdir -p "$(dirname "$expected")"
        cp "$actual" "$expected"
        pass=$((pass + 1))
    elif [ -f "$expected" ] && cmp -s "$actual" "$expected"; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL: $name"
        if [ -f "$expected" ]; then
            diff "$expected" "$actual" | head -15
        else
            echo "  missing golden file $expected"
        fi
    fi
}

for src in "$root"/examples/*.mc; do
    base=$(basename "$src" .mc)
    dir=$(dirname "$src")
    capture "$dir" "$base.mc" "$work/actual" "$@"
    verify "examples/$base run" "$work/actual" "$golden/examples/$base.out"
    capture "$dir" "$base.mc" "$work/actual" "$@" --tac --no-run
    verify "examples/$base tac" "$work/actual" "$golden/examples/$base.tac"
    capture "$dir" "$base.mc" "$work/actual" "$@" -O0 --tac --no-run
    verify "examples/$base tac -O0" "$work/actual" "$golden/examples/$base.tac0"
    capture "$dir" "$base.mc" "$work/actual" "$@" --bytecode --no-run
    verify "examples/$base bytecode" "$work/actual" "$golden/examples/$base.bc"
    capture "$dir" "$base.mc" "$work/actual" "$@" --tokens --ast --symbols --no-run
    verify "examples/$base dump" "$work/actual" "$golden/examples/$base.dump"
    capture "$dir" "$base.mc" "$work/actual" "$@" -O0
    verify "examples/$base run -O0" "$work/actual" "$golden/examples/$base.out0"
done

for src in "$root"/tests/cases/*.mc; do
    base=$(basename "$src" .mc)
    dir=$(dirname "$src")
    capture "$dir" "$base.mc" "$work/actual" "$@" --max-steps 100000
    verify "cases/$base" "$work/actual" "$golden/cases/$base.out"
done

echo "golden: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
