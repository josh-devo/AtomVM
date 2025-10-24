# AtomVM WASI File I/O Status

## Current Status: ❌ NOT IMPLEMENTED

File I/O operations are not yet implemented for AtomVM WASI. The `file` module functions are not available.

## What's Needed

To support file I/O in AtomVM WASI, the following would need to be implemented:

### Required Erlang file module functions:
- `file:write_file/2` - Write binary data to a file
- `file:read_file/1` - Read entire file into binary
- `file:read_file_info/1` - Get file metadata (size, type, permissions)
- `file:delete/1` - Delete a file
- `file:open/2` - Open file for streaming I/O
- `file:read/2` - Read from open file
- `file:write/2` - Write to open file
- `file:close/1` - Close file handle

### WASI APIs to use:
```c
// From wasi:filesystem/types@0.2.0
descriptor_open_at()
descriptor_read_via_stream()
descriptor_write_via_stream()
descriptor_stat()
descriptor_unlink_file_at()

// From wasi:io/streams@0.2.0
input_stream_blocking_read()
output_stream_blocking_write()
output_stream_blocking_flush()
```

### Implementation Approach

1. **Create file driver** (`src/platforms/wasi/src/lib/wasi_file.c`)
   - Port driver similar to `wasi_net.c`
   - Handle file operations via WASI filesystem APIs
   - Support both synchronous and streaming I/O

2. **Register with estdlib**
   - Add file driver to WASI platform initialization
   - Ensure `file` module can find the driver

3. **Handle preopened directories**
   - WASI requires files to be accessed relative to preopened directories
   - The `--dir=.::` wasmtime flag grants access to current directory

## Test Files Created

The following test files are ready to use once file I/O is implemented:

### `test_file_io.erl`
Basic file I/O test covering:
- Writing files with `file:write_file/2`
- Reading files with `file:read_file/1`
- Getting file info with `file:read_file_info/1`

**Run it:**
```bash
erlc test_file_io.erl
~/.wasmtime/bin/wasmtime run --dir=.::. \\
  ../../build-wasi/src/platforms/wasi/AtomVM.wasm \\
  estdlib.avm test_file_io.beam
```

### `test_file_sharing.ex` (Future)
File-based IPC test demonstrating:
- Multiple processes communicating via shared files
- Simple file locking mechanism
- Writer/reader pattern

## WASI File Access Model

WASI uses a capability-based security model:

1. **Preopened Directories**: The host grants access to specific directories
   ```bash
   wasmtime --dir=.::. program.wasm      # Current dir as "."
   wasmtime --dir=/tmp::/scratch program.wasm  # /tmp as "/scratch"
   ```

2. **No Absolute Paths**: All file access is relative to preopened dirs

3. **Explicit Permissions**: Each preopened dir has specific permissions (read, write, etc.)

## Next Steps

To implement file I/O support:

1. Study the WASI filesystem bindings in WASI SDK headers
2. Create a file port driver similar to the socket driver
3. Implement file module NIFs that delegate to the port driver
4. Test with the provided test files

## References

- [WASI Filesystem Spec](https://github.com/WebAssembly/wasi-filesystem)
- [WASI I/O Streams](https://github.com/WebAssembly/wasi-io)
- AtomVM socket driver implementation: `src/platforms/wasi/src/lib/wasi_net.c`
