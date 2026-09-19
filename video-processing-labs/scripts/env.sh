# Source from MSYS2 UCRT64 bash: source scripts/env.sh
MSYS="${MIMO_MSYS2_ROOT:-/c/msys64}"
export PATH="$MSYS/ucrt64/bin:$MSYS/usr/bin:$PATH"
echo "MSYS2 UCRT64 ready: $(command -v gcc)"
