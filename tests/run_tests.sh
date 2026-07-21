#!/bin/bash
# ELang smoke tests
set -e

ELC="./bin/elc"
TMPDIR=$(mktemp -d)
PASS=0
FAIL=0

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

run_test() {
    local name="$1"
    local file="$2"
    local expected="$3"

    local out="$TMPDIR/$name"
    if ! $ELC -o "$out" "$file" 2>/dev/null; then
        echo "FAIL [compile] $name"
        FAIL=$((FAIL + 1))
        return
    fi
    local got
    got=$("$out" 2>/dev/null) || true
    if [ "$got" = "$expected" ]; then
        echo "PASS $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL $name"
        echo "  expected: $expected"
        echo "  got:      $got"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== ELang smoke tests ==="
echo ""

# Phase 1 tests
run_test "hello" "examples/01_hello_world.el" "Hello, ELang!"

# Phase 2 tests: loop, break, continue
run_test "loop" "test_loop.el" "$(printf '5\n52\n10')"

# Existing examples (just verify they compile and run)
for f in examples/0[2-9]_*.el examples/1[0-9]_*.el; do
    name=$(basename "$f" .el)
    out="$TMPDIR/$name"
    if $ELC -o "$out" "$f" 2>/dev/null; then
        "$out" >/dev/null 2>&1 && { echo "PASS $name (compile+run)"; PASS=$((PASS + 1)); } \
            || { echo "FAIL $name (runtime)"; FAIL=$((FAIL + 1)); }
    else
        echo "FAIL $name (compile)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
