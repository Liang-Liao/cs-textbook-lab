#!/bin/sh
# Run every lab's make test from the repo root.
# Invoked as: sh scripts/run_tests.sh   (cwd = repo root)

if ! [ -f common/Makefile ]; then
  echo "error: run from repository root" >&2
  exit 1
fi

if command -v mingw32-make >/dev/null 2>&1; then
  MAKE=mingw32-make
elif command -v make >/dev/null 2>&1; then
  MAKE=make
else
  echo "error: make/mingw32-make not found" >&2
  exit 1
fi

fail=0
found=0
for lab in labs/ch*/; do
  [ -f "${lab}Makefile" ] || continue
  found=$((found + 1))
  echo "======== $lab ========"
  if "$MAKE" -C "$lab" test; then
    echo "OK $lab"
  else
    echo "FAIL $lab"
    fail=1
  fi
done

if [ "$found" -eq 0 ]; then
  echo "error: no labs found under labs/" >&2
  exit 1
fi

if [ "$fail" -ne 0 ]; then
  echo "Some labs FAILED"
  exit 1
fi
echo "All labs passed ($found)."
