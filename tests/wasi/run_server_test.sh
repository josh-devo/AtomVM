#!/bin/bash
lsof -ti :8081 | xargs -r kill -9 2>/dev/null
sleep 1

timeout 10 ~/.wasmtime/bin/wasmtime run -Sinherit-network -Stcp -Sallow-ip-name-lookup --dir=.::. ../../build-wasi/src/platforms/wasi/AtomVM.wasm estdlib.avm exavmlib.avm port.beam Elixir.TestTcpServer.beam 2>&1 &
SERVER_PID=$!

sleep 2

echo "[CLIENT] Sending 'Hello Server' to port 8081..."
echo "Hello Server" | nc localhost 8081
echo "[CLIENT] Got response from server"

sleep 1
wait $SERVER_PID
EXIT_CODE=$?
echo "Server exit code: $EXIT_CODE"
