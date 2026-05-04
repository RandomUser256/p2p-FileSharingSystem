#!/usr/bin/env bash
# run_tests.sh — Build and run the Chord DHT test suite
#
# Usage:
#   ./tests/run_tests.sh <node_ip> [node_ip ...] [--ui]
#
#   node_ip   IP of a running Chord node (port 8080)
#   --ui      also run the CLI responsiveness test (requires nodeInfo/Node)
#
# The script must be called from the project root (where main.c lives).
#
# Example — two nodes, with UI test:
#   ./tests/run_tests.sh 10.11.20.37 10.11.20.38 --ui

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TEST_SRC="$PROJECT_DIR/tests/test_chord.c"
TEST_BIN="$PROJECT_DIR/tests/test_chord"
MAIN_BIN="$PROJECT_DIR/main"

# ---- argument parsing ----
IPS=()
DO_UI=0

for arg in "$@"; do
    if [[ "$arg" == "--ui" ]]; then
        DO_UI=1
    else
        IPS+=("$arg")
    fi
done

if [[ ${#IPS[@]} -eq 0 ]]; then
    echo "Usage: $0 <node_ip> [node_ip ...] [--ui]" >&2
    exit 1
fi

# ---- build test binary ----
echo "==> Building test binary..."
gcc -Wall -Wextra -O0 -g "$TEST_SRC" -o "$TEST_BIN"
echo "    OK: $TEST_BIN"

# ---- optionally build main if absent ----
if [[ "$DO_UI" -eq 1 && ! -f "$MAIN_BIN" ]]; then
    echo "==> Building main binary..."
    cd "$PROJECT_DIR"
    gcc main.c src/node.c src/DHASH.c src/maintenance.c \
        src/logger.c src/tcpServer.c \
        -o main -pthread -lm
    echo "    OK: $MAIN_BIN"
fi

# ---- run tests ----
echo ""
echo "==> Nodes under test: ${IPS[*]}"
echo ""

if [[ "$DO_UI" -eq 1 ]]; then
    "$TEST_BIN" "${IPS[@]}" --ui "$MAIN_BIN" "$PROJECT_DIR"
else
    "$TEST_BIN" "${IPS[@]}"
fi
