#!/bin/bash

# Interactive host that sends events to WASM and reacts to responses in real-time

WASM_CMD="~/.wasmtime/bin/wasmtime run --dir=.::. ../../build-wasi/src/platforms/wasi/AtomVM.wasm estdlib.avm event_processor.beam"

echo "=== INTERACTIVE HOST STARTED ==="
echo "Starting WASM event processor..."
echo ""

# Create a named pipe for sending input
FIFO="/tmp/wasm_events_$$"
mkfifo "$FIFO" 2>/dev/null || true

# Start WASM process in background, monitoring its output
(eval "$WASM_CMD" < "$FIFO") | while IFS= read -r line; do
    echo "[HOST RECEIVED] $line"

    # Parse and react to events
    if [[ "$line" =~ "event_processed" ]]; then
        # Extract event number
        if [[ "$line" =~ num:\ ([0-9]+) ]]; then
            event_num="${BASH_REMATCH[1]}"
            echo "  → Host: Acknowledged event #$event_num"
        fi
    elif [[ "$line" =~ "type: stats" ]]; then
        echo "  → Host: Statistics received, analysis complete"
    fi
done &
MONITOR_PID=$!

# Give WASM time to start
sleep 1

# Open pipe for writing
exec 3>"$FIFO"

echo "[HOST] Sending event: user_login:alice"
echo "user_login:alice" >&3
sleep 0.5

echo "[HOST] Sending event: data_update:temperature=25C"
echo "data_update:temperature=25C" >&3
sleep 0.5

echo "[HOST] Sending event: notification:hello_from_host"
echo "notification:hello_from_host" >&3
sleep 0.5

echo "[HOST] Sending event: api_call:get_data"
echo "api_call:get_data" >&3
sleep 0.5

echo "[HOST] Requesting statistics..."
echo "STATS" >&3
sleep 0.5

echo "[HOST] Sending shutdown command..."
echo "QUIT" >&3

# Close pipe
exec 3>&-

# Wait for monitor to finish
wait $MONITOR_PID 2>/dev/null

# Cleanup
rm -f "$FIFO"

echo ""
echo "=== HOST: Session complete ==="
