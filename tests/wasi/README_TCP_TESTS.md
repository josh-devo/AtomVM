# AtomVM WASI TCP Socket Tests

This directory contains TCP socket tests for AtomVM running on WASI (WebAssembly System Interface).

## Phase 1: TCP Client Support ✅ COMPLETE

The following TCP client functionality is fully working:

### Erlang Client Test: `test_tcp_socket.erl`
Simple TCP client that connects to an echo server.

**Run it:**
```bash
# Start echo server
socat TCP-LISTEN:8080,reuseaddr,fork EXEC:cat &

# Run Erlang test
~/.wasmtime/bin/wasmtime run \
  -Sinherit-network -Stcp -Sallow-ip-name-lookup \
  --dir=.::. ../../build-wasi/src/platforms/wasi/AtomVM.wasm \
  estdlib.avm exavmlib.avm port.beam test_tcp_socket.beam
```

### Elixir Client Test: `test_tcp_socket.ex`
Idiomatic Elixir version using `with` for clean error handling.

**Run it:**
```bash
# Ensure echo server is running (see above)

# Run Elixir test
~/.wasmtime/bin/wasmtime run \
  -Sinherit-network -Stcp -Sallow-ip-name-lookup \
  --dir=.::. ../../build-wasi/src/platforms/wasi/AtomVM.wasm \
  estdlib.avm exavmlib.avm port.beam Elixir.TestTcpSocket.beam
```

### What Works
- ✅ `gen_tcp:connect/3` and `gen_tcp:connect/4`
- ✅ `gen_tcp:send/2` with automatic flush
- ✅ `gen_tcp:recv/2` and `gen_tcp:recv/3` with blocking I/O
- ✅ `gen_tcp:close/1`
- ✅ Binary and list mode
- ✅ Active and passive modes
- ✅ Connection timeout handling

### Implementation Details

**WASI Preview 2 Async Networking:**
- Connections require polling `finish_connect()` until completion
- Reads use `blocking_read()` to wait for data
- Writes use `blocking_flush()` to ensure data is sent

**Key Files:**
- `src/platforms/wasi/src/lib/wasi_net.c` - WASI socket driver
- `libs/estdlib/src/gen_tcp.erl` - gen_tcp API with `connect/4` added
- `cmake/wasi-toolchain.cmake` - WASI SDK configuration

## Phase 2: TCP Server Support 🚧 TODO

Server functionality is not yet implemented. The following will be added:

### Planned Elixir Server Test: `test_tcp_server.ex`
Echo server that listens, accepts connections, and echoes data back.

**Status:** Code written, but returns `{error, not_supported}`

**What Needs Implementation:**
- ❌ `gen_tcp:listen/2` - Create listening socket
- ❌ `gen_tcp:accept/1` and `gen_tcp:accept/2` - Accept client connections
- ❌ Message handler for `listen` command
- ❌ Message handler for `accept` command
- ❌ WASI Preview 2 `start_listen()` and `finish_listen()` calls
- ❌ WASI Preview 2 `accept()` with polling for async operation

**WASI APIs Needed:**
```c
// From wasi:sockets/tcp@0.2.0
bool tcp_method_tcp_socket_start_bind(
    tcp_borrow_tcp_socket_t self,
    network_borrow_network_t network,
    network_ip_socket_address_t *local_address,
    tcp_error_code_t *err
);

bool tcp_method_tcp_socket_finish_bind(
    tcp_borrow_tcp_socket_t self,
    tcp_error_code_t *err
);

bool tcp_method_tcp_socket_start_listen(
    tcp_borrow_tcp_socket_t self,
    tcp_error_code_t *err
);

bool tcp_method_tcp_socket_finish_listen(
    tcp_borrow_tcp_socket_t self,
    tcp_error_code_t *err
);

bool tcp_method_tcp_socket_accept(
    tcp_borrow_tcp_socket_t self,
    tcp_tuple3_own_tcp_socket_own_input_stream_own_output_stream_t *ret,
    tcp_error_code_t *err
);
```

## Running the Tests

### Prerequisites
1. WASI SDK 22.0 installed at `/home/joshadams/wasi-sdk-22.0`
2. wasmtime runtime with WASI Preview 2 support
3. AtomVM built for WASI target

### Build Commands
```bash
# Build AtomVM for WASI
cd /home/joshadams/src/github.com/atomvm/AtomVM
export WASI_SDK_PATH=/home/joshadams/wasi-sdk-22.0
just wasi-build

# Compile test files
cd tests/wasi
erlc test_tcp_socket.erl
elixirc test_tcp_socket.ex
elixirc test_tcp_server.ex
```

### Required wasmtime Flags
- `-Sinherit-network` - Grant network access
- `-Stcp` - Enable TCP sockets
- `-Sallow-ip-name-lookup` - Enable DNS resolution
- `--dir=.::` - Mount current directory

## Architecture Notes

### WASI Preview 2 Component Model
AtomVM WASI uses the Component Model for structured imports:
- `wasi:sockets/tcp@0.2.0` - TCP socket operations
- `wasi:sockets/network@0.2.0` - Network instance
- `wasi:io/streams@0.2.0` - Byte stream I/O
- `wasi:io/poll@0.2.0` - Async I/O polling

### Async I/O Pattern
WASI Preview 2 sockets are async by nature:
1. Start operation (e.g., `start_connect()`)
2. Poll completion (e.g., `finish_connect()`)
3. Retry on WOULD_BLOCK error
4. Process result when ready

### Driver Architecture
```
Erlang/Elixir Code
    ↓
gen_tcp module
    ↓
Port messages
    ↓
wasi_net driver (wasi_net.c)
    ↓
WASI Preview 2 APIs
    ↓
wasmtime runtime
    ↓
Host OS networking
```

## Test Results

### Client Tests (Phase 1)
```
✅ Erlang client: ok, exit code 0
✅ Elixir client: ok, exit code 0
```

### Server Test (Phase 2)
```
❌ Elixir server: {error, not_supported}
   (waiting for listen/accept implementation)
```

## Contributing

To implement Phase 2 server support:

1. Add message handlers in `wasi_socket_consume_mailbox()`:
   - `listen` command
   - `accept` command

2. Implement `wasi_socket_driver_do_listen()` function:
   - Parse port and options
   - Call `start_bind()` with address
   - Poll `finish_bind()` until ready
   - Call `start_listen()`
   - Poll `finish_listen()` until ready
   - Set `listening = true`

3. Implement `wasi_socket_driver_do_accept()` function:
   - Poll `tcp_socket_accept()` until connection arrives
   - Create new socket context for client
   - Return socket handle

4. Add error code mappings for bind/listen errors

5. Test with the provided `test_tcp_server.ex`
