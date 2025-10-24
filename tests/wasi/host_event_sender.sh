#!/bin/bash

# Host-side event sender that communicates with WASM GenServer
# Sends events via stdin and processes responses from stdout in real-time

WASM_CMD="~/.wasmtime/bin/wasmtime run --dir=.::. ../../build-wasi/src/platforms/wasi/AtomVM.wasm estdlib.avm exavmlib.avm Elixir.InteractiveDemo.beam"

echo "=== HOST EVENT SENDER ==="
echo "Starting WASM GenServer and sending events..."
echo ""

# Create a named pipe for bidirectional communication
mkfifo /tmp/wasm_input 2>/dev/null || true

# Start background process to monitor output
(
  eval "$WASM_CMD" < /tmp/wasm_input | while IFS= read -r line; do
    echo "[HOST RECEIVED] $line"
  done
) &
WASM_PID=$!

# Give WASM time to start
sleep 1

# Send events via the pipe
exec 3>/tmp/wasm_input

echo "Sending event 1..."
echo "user_login:alice" >&3
sleep 0.5

echo "Sending event 2..."
echo "data_update:temperature=25C" >&3
sleep 0.5

echo "Sending event 3..."
echo "notification:hello_from_host" >&3
sleep 0.5

echo "Requesting stats..."
echo "STATS" >&3
sleep 0.5

echo "Sending quit..."
echo "QUIT" >&3

# Close the pipe and wait for WASM to finish
exec 3>&-
wait $WASM_PID

# Cleanup
rm -f /tmp/wasm_input

echo ""
echo "=== HOST: All events processed ==="
