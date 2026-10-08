#!/usr/bin/env bash
set -euo pipefail

SRC_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$SRC_ROOT/src"

MODE="release"
MAKE_ARGS=()

for arg in "$@"; do
    case "$arg" in
        debug|-DEBUG|--debug|DEBUG=1)
            MODE="debug"
            MAKE_ARGS+=(DEBUG=1)
            ;;
        clean)
            make clean
            exit 0
            ;;
        *)
            MAKE_ARGS+=("$arg")
            ;;
    esac
done

echo "=========================================================="
if [ "$MODE" = "debug" ]; then
    echo "  Building FreeArc Native (DEBUG mode)"
else
    echo "  Building FreeArc Native (PRODUCTION - Aggressive)"
fi
echo "=========================================================="

make clean >/dev/null 2>&1 || true
make -j"$(nproc)" "${MAKE_ARGS[@]}"

echo ""
echo "=== Summary of Generated Binaries ==="
ls -lh "$SRC_ROOT/build/linux/arc" "$SRC_ROOT/build/linux/unarc" "$SRC_ROOT/build/linux/arc.sfx"
echo "Done."
