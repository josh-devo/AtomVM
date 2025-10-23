# AtomVM WASI Implementation

This directory contains the WASI (WebAssembly System Interface) platform implementation for AtomVM, enabling server-side WebAssembly execution in runtimes like Wasmtime and Wasmex.

## Quick Start

```bash
# Install WASI SDK and build
just setup
just build

# Run tests
just test

# Create optimized release build
just release
```

## What is WASI?

WASI (WebAssembly System Interface) is a standardized system interface for WebAssembly that provides:
- **Portable**: Run the same WASM binary across different runtimes
- **Sandboxed**: Capability-based security model
- **Fast**: Near-native execution speed
- **Server-side**: No browser required

## Why WASI for AtomVM?

The existing Emscripten build targets browsers and NodeJS. WASI enables:
- ✅ **Elixir Integration**: Run AtomVM from Elixir via [Wasmex](https://hex.pm/packages/wasmex)
- ✅ **Edge Computing**: Deploy to edge runtimes (Cloudflare Workers, Fastly Compute@Edge)
- ✅ **Standalone Execution**: Run with wasmtime, wasmer, or other WASI runtimes
- ✅ **Embedded Systems**: Lightweight WASM execution without JavaScript
- ✅ **Plugin Systems**: Sandboxed Erlang/Elixir plugins

## Project Structure

```
AtomVM/
├── Justfile                       # Build automation (WASI-specific commands)
├── atomvm_wasi_plan.md            # Detailed implementation plan (25-40 days)
├── WASI_DEPENDENCY_AUDIT.md      # Emscripten → WASI dependency mapping
├── WASI_README.md                 # This file
├── cmake/
│   └── wasi-toolchain.cmake       # CMake toolchain for WASI (auto-generated)
├── src/platforms/wasi/            # WASI platform implementation (to be created)
│   ├── platform.h                 # Platform types and capabilities
│   ├── sys_wasi.c                 # System calls and file I/O
│   ├── memory_wasi.c              # Memory management
│   ├── time_wasi.c                # Timing functions
│   ├── random_wasi.c              # Random number generation
│   ├── exports.c                  # WASM export interface
│   └── main.c                     # Entry point
├── tools/
│   └── wasi-sdk-22.0/             # WASI SDK (installed by `just setup`)
└── build-wasi/                    # Build artifacts
    └── src/
        └── AtomVM.wasm            # Final WASM binary
```

## Build System

### Prerequisites

- CMake 3.20+
- wget or curl (for downloading WASI SDK)
- [just](https://github.com/casey/just) command runner (optional but recommended)

### Using Just (Recommended)

```bash
# Show all available commands
just --list

# Complete setup (downloads WASI SDK, creates toolchain)
just setup

# Build AtomVM for WASI
just build

# Show build information
just info

# Clean and rebuild
just dev

# Create optimized release build
just release

# Run tests with wasmtime
just test
```

### Manual Build

```bash
# Download and extract WASI SDK
mkdir -p tools
cd tools
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-22/wasi-sdk-22.0-linux.tar.gz
tar xzf wasi-sdk-22.0-linux.tar.gz
cd ..

# Configure CMake
mkdir build-wasi
cd build-wasi
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/wasi-toolchain.cmake \
    -DAVM_PLATFORM_WASI=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVM_DISABLE_SMP=ON \
    -DAVM_DISABLE_NETWORKING=ON

# Build
make -j$(nproc)

# Output: build-wasi/src/AtomVM.wasm
```

## Usage

### Standalone with Wasmtime

```bash
# Install wasmtime
curl https://wasmtime.dev/install.sh -sSf | bash

# Run AtomVM WASM
wasmtime --dir=. build-wasi/src/AtomVM.wasm
```

### From Elixir with Wasmex

```elixir
# mix.exs
def deps do
  [
    {:wasmex, "~> 0.8"}
  ]
end

# Load and run AtomVM
{:ok, bytes} = File.read("build-wasi/src/AtomVM.wasm")
{:ok, module} = Wasmex.Module.from_bytes(bytes)
{:ok, instance} = Wasmex.Instance.new(module)

# Call exported functions
{:ok, [result]} = Wasmex.Instance.call_exported_function(instance, :avm_init, [])
```

### From Rust with wasmtime

```rust
use wasmtime::*;

fn main() -> Result<()> {
    let engine = Engine::default();
    let module = Module::from_file(&engine, "AtomVM.wasm")?;
    let mut store = Store::new(&engine, ());
    let instance = Instance::new(&mut store, &module, &[])?;

    // Call exports
    let avm_init = instance.get_typed_func::<(), i32>(&mut store, "avm_init")?;
    let result = avm_init.call(&mut store, ())?;

    Ok(())
}
```

## Features & Limitations

### Supported Features

- ✅ BEAM bytecode execution
- ✅ File I/O via preopened directories
- ✅ Standard I/O (stdin/stdout/stderr)
- ✅ Timing functions (monotonic and wall clock)
- ✅ Cryptographically secure random numbers
- ✅ Memory management
- ✅ Environment variables

### Current Limitations (WASI Preview 1)

- ❌ **No Threading**: Single-threaded only (`AVM_NO_SMP`)
- ❌ **No Networking**: No sockets, HTTP, or WebSockets
- ❌ **No Browser Features**: No HTML events or JavaScript interop
- ❌ **Limited Port Drivers**: Most ports require OS-specific features

### Future (WASI Preview 2+)

WASI Preview 2 and component model may add:
- Threading support (wasi-threads)
- Networking (wasi-sockets)
- Async I/O

## Development Workflow

### Project Management with `bd`

The project uses [bd](https://github.com/yourusername/bd) for issue tracking:

```bash
# Show ready-to-work issues
bd ready

# Show all issues
bd list

# Update task status
bd update wasi-4 --status in_progress

# Close completed task
bd close wasi-4 --reason "Completed platform.h"

# Show dependency tree
bd dep tree wasi-1
```

### Implementation Status

Track progress in `.beads/wasi.db` (SQLite database).

**Current Status**: Phase 1 - Setup & Research
- ✅ wasi-2: WASI SDK installed
- ✅ wasi-3: Dependencies audited
- ⏳ wasi-4: Platform abstraction design (next)
- ⏳ wasi-5: CMake toolchain

See `atomvm_wasi_plan.md` for detailed roadmap.

## Testing

### Unit Tests

```bash
# Build with tests
cmake .. -DAVM_PLATFORM_WASI=ON -DBUILD_TESTS=ON
make
ctest
```

### Integration Tests

```bash
# Test WASM validity
wasmtime validate build-wasi/src/AtomVM.wasm

# Test basic execution
wasmtime build-wasi/src/AtomVM.wasm test.avm

# Test with Wasmex
mix test
```

### Performance Benchmarks

```bash
# Compare with native
hyperfine 'wasmtime AtomVM.wasm test.avm' './atomvm test.avm'

# Measure WASM size
ls -lh build-wasi/src/AtomVM.wasm

# Profile with wasmtime
wasmtime run --profile build-wasi/src/AtomVM.wasm
```

## Architecture

### Platform Abstraction

WASI platform implements the AtomVM `sys.h` interface:

```c
// Core system functions
void sys_init_platform(GlobalContext *glb);
void sys_free_platform(GlobalContext *glb);
void sys_poll_events(GlobalContext *glb, int timeout_ms);

// File I/O
enum OpenAVMResult sys_open_avm_from_file(GlobalContext *global, const char *path, struct AVMPackData **data);
Module *sys_load_module_from_file(GlobalContext *global, const char *path);

// Time
void sys_time(struct timespec *t);
void sys_monotonic_time(struct timespec *t);
uint64_t sys_monotonic_time_u64(void);

// Random
int sys_mbedtls_entropy_func(void *entropy, unsigned char *buf, size_t size);
```

### WASM Exports

The WASI build exposes these functions:

```wasm
(export "avm_init" (func $avm_init))             ; Initialize AtomVM
(export "execute" (func $execute))                ; Execute with input/output
(export "get_result_len" (func $get_result_len)) ; Get result length
(export "free_result" (func $free_result))       ; Free result memory
(export "_start" (func $_start))                  ; WASI entry point
```

### Memory Layout

```
┌─────────────────────────────────┐
│ Static Data                     │
├─────────────────────────────────┤
│ Embedded AVM (optional)         │ ← _binary_app_avm_start
├─────────────────────────────────┤
│ Heap (malloc/free)              │ ← Managed by WASI libc
│         ↓ grows down            │
├─────────────────────────────────┤
│ AtomVM Global Context           │
├─────────────────────────────────┤
│ BEAM Process Heaps              │
│         ↓ grows down            │
└─────────────────────────────────┘
         WASM Linear Memory
```

## Documentation

- **[atomvm_wasi_plan.md](./atomvm_wasi_plan.md)**: Comprehensive 25-40 day implementation plan
- **[WASI_DEPENDENCY_AUDIT.md](./WASI_DEPENDENCY_AUDIT.md)**: Detailed Emscripten→WASI mapping
- **[Justfile](./Justfile)**: All build automation commands

## Contributing

### Code Style

Follow AtomVM's existing style:
- K&R brace style
- 4-space indentation
- `snake_case` for functions
- `CamelCase` for types

### Pull Request Process

1. Create feature branch: `git checkout -b wasi-feature-name`
2. Implement changes
3. Test: `just test`
4. Format: `just format` (if available)
5. Commit with clear messages
6. Open PR against `main` branch

### Testing Requirements

- All new functions must have unit tests
- Integration tests for WASI-specific features
- Performance tests for critical paths
- Documentation for all public APIs

## Resources

### WASI

- [WASI Specification](https://github.com/WebAssembly/WASI)
- [WASI SDK](https://github.com/WebAssembly/wasi-sdk)
- [wasi-libc](https://github.com/WebAssembly/wasi-libc)
- [WASI Preview 1 API](https://github.com/WebAssembly/WASI/blob/main/phases/snapshot/docs.md)

### Runtimes

- [Wasmtime](https://wasmtime.dev/) - Fast, secure WebAssembly runtime
- [Wasmer](https://wasmer.io/) - Universal WebAssembly runtime
- [Wasmex](https://hex.pm/packages/wasmex) - Elixir WASM runtime
- [wasm3](https://github.com/wasm3/wasm3) - Fast WebAssembly interpreter

### Tools

- [wasm-opt](https://github.com/WebAssembly/binaryen) - WebAssembly optimizer
- [wasm-strip](https://github.com/WebAssembly/wabt) - Strip debug info
- [just](https://github.com/casey/just) - Command runner

### AtomVM

- [AtomVM Repository](https://github.com/atomvm/AtomVM)
- [AtomVM Documentation](https://atomvm.net/)
- [AtomVM Book](https://atomvm.github.io/AtomVM-book/)

## Support

- **Issues**: Use `bd` for project tracking, GitHub Issues for bugs/features
- **Discussions**: [AtomVM Discussions](https://github.com/atomvm/AtomVM/discussions)
- **Community**: Elixir Forum, WASI Discord

## License

Same as AtomVM: Apache-2.0 OR LGPL-2.1-or-later

---

**Status**: 🚧 In Development - Phase 1 Complete (Setup & Research)

**Next Steps**: Implement platform abstraction layer (wasi-4, wasi-5)
