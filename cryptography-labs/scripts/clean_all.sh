#!/bin/sh
# Clean common + all labs. Invoke from repo root.
if ! [ -f common/Makefile ]; then
  echo "error: run from repository root" >&2
  exit 1
fi
if command -v mingw32-make >/dev/null 2>&1; then MAKE=mingw32-make; else MAKE=make; fi
"$MAKE" -C common clean
for lab in labs/ch*/; do
  [ -f "${lab}Makefile" ] || continue
  "$MAKE" -C "$lab" clean
done
echo "clean done."
