# AtomVM WASI Implementation Plan

**Goal**: Add WASI (WebAssembly System Interface) support to AtomVM to enable server-side WASM execution in runtimes like Wasmex.

**Status**: AtomVM currently only supports Emscripten (browser/NodeJS). WASI support is acknowledged as needed by maintainers but not yet implemented.

---

## Executive Summary

### Current State
- **AtomVM**: BEAM VM (Erlang/Elixir interpreter) written in C
- **Current Target**: Emscripten → Browser/NodeJS via WebAssembly
- **Problem**: Emscripten WASM incompatible with WASI runtimes (Wasmtime, Wasmex)

### Required Changes
- **Core**: ~1250 lines of code across 15-20 files
- **Effort**: 25-40 days of focused C/WASM development
- **Complexity**: Medium-High (platform abstraction layer + build system)

### Why WASI?
- ✅ Server-side WASM execution (Elixir via Wasmex, Rust via wasmtime)
- ✅ Standardized system interface (POSIX-like)
- ✅ Sandboxed, portable, fast
- ✅ No JavaScript runtime dependency

---

## Architecture Analysis

### Gap: Emscripten vs WASI

| Feature | Emscripten | WASI | Migration Complexity |
|---------|------------|------|---------------------|
| Memory Growth | `emscripten_resize_heap()` | `__builtin_wasm_memory_grow()` | Medium |
| Timing | `emscripten_get_now()` | `clock_time_get()` | Easy |
| File I/O | Virtual FS (open/read/write) | Capability-based (`fd_*`) | Hard |
| Random | `getentropy()` | `random_get()` | Easy |
| Environment | `getenv()` | `environ_get()` | Easy |
| Networking | ❌ Not used | ❌ Not in WASI Preview 1 | N/A |
| Threading | SharedArrayBuffer | ❌ Not in WASI Preview 1 | Skip for MVP |

### AtomVM Dependencies on Emscripten

From error logs and source analysis:
```
unknown import: `env::emscripten_resize_heap` has not been defined
```

**Known Emscripten Imports:**
- `env::emscripten_resize_heap` - Memory growth
- `env::emscripten_get_now` - Monotonic clock (likely)
- `env::emscripten_notify_memory_growth` - Heap notification (likely)
- POSIX wrappers: `__syscall_*` functions
- Virtual filesystem operations

**These must be replaced with WASI equivalents.**

---

## Implementation Approaches

### Approach 1: Full WASI Port (Recommended)
**Effort**: 25-40 days | **Risk**: Medium | **Quality**: High

Add WASI as a first-class platform alongside Emscripten.

**Pros:**
- ✅ Clean architecture
- ✅ Maintainable long-term
- ✅ Upstream contribution potential
- ✅ 100% compatibility with WASI runtimes

**Cons:**
- ❌ Most time-intensive
- ❌ Requires deep AtomVM knowledge
- ❌ Build system complexity

---

### Approach 2: Minimal BEAM Executor
**Effort**: 15-20 days | **Risk**: Medium | **Quality**: Medium

Build stripped-down interpreter for specific use cases (e.g., JSON validation).

**Pros:**
- ✅ Faster development
- ✅ Smaller WASM binary
- ✅ Focused on needed features

**Cons:**
- ❌ Not general-purpose
- ❌ Limited BEAM instruction support
- ❌ Diverges from upstream AtomVM

---

### Approach 3: Emscripten Shim Layer (Not Recommended)
**Effort**: 15-20 days | **Risk**: High | **Quality**: Low

Implement Emscripten APIs using WASI primitives.

**Pros:**
- ✅ No AtomVM source changes

**Cons:**
- ❌ Fragile, hard to maintain
- ❌ Still need to resolve ALL imports
- ❌ Doesn't solve architectural mismatch
- ❌ Poor long-term sustainability

---

## Detailed Plan: Approach 1 (Full WASI Port)

### Phase 1: Setup & Research (3-4 days)

#### Day 1: Environment Setup
```bash
# Install WASI SDK
cd /opt
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-22/wasi-sdk-22.0-linux.tar.gz
tar xzf wasi-sdk-22.0-linux.tar.gz
export WASI_SDK_PATH=/opt/wasi-sdk-22.0

# Clone AtomVM
git clone https://github.com/atomvm/AtomVM.git
cd AtomVM
git checkout -b wasi-support
```

#### Day 2: Dependency Mapping
- Audit all Emscripten-specific code
- List all imported `env::*` functions
- Map each to WASI equivalent
- Document unsupported features (networking, threading)

**Key files to audit:**
```
src/libAtomVM/sys.c           # System calls
src/libAtomVM/memory.c        # Memory management
src/libAtomVM/scheduler.c     # Process scheduling
src/platforms/emscripten/     # Current Emscripten platform
```

#### Day 3: Architecture Design
Create platform abstraction layer design:

```
src/platforms/
├── emscripten/          # Existing
├── generic_unix/        # Existing
├── esp32/               # Existing
└── wasi/                # NEW
    ├── platform.h       # Platform-specific types
    ├── sys_wasi.c       # System calls
    ├── memory_wasi.c    # Memory management
    ├── time_wasi.c      # Timing functions
    ├── random_wasi.c    # Random number generation
    └── main.c           # Entry point
```

#### Day 4: Build System Setup
Create CMake toolchain file:

```cmake
# cmake/wasi-toolchain.cmake
set(CMAKE_SYSTEM_NAME WASI)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

set(WASI_SDK_PREFIX $ENV{WASI_SDK_PATH})
set(CMAKE_C_COMPILER ${WASI_SDK_PREFIX}/bin/clang)
set(CMAKE_SYSROOT ${WASI_SDK_PREFIX}/share/wasi-sysroot)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --target=wasm32-wasi")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-undefined")
```

---

### Phase 2: Core Platform Implementation (10-14 days)

#### Days 5-6: Platform Abstraction Layer

**File: `src/platforms/wasi/platform.h`**
```c
#ifndef PLATFORM_WASI_H
#define PLATFORM_WASI_H

#include <wasi/api.h>
#include <stdint.h>
#include <stdbool.h>

// Platform-specific types
typedef __wasi_fd_t avm_file_t;
typedef __wasi_timestamp_t avm_timestamp_t;

// Platform capabilities
#define PLATFORM_HAS_FILESYSTEM 1
#define PLATFORM_HAS_NETWORKING 0
#define PLATFORM_HAS_THREADING 0

#endif // PLATFORM_WASI_H
```

**File: `src/platforms/wasi/time_wasi.c`**
```c
#include "platform.h"
#include <wasi/api.h>

// Replace emscripten_get_now()
uint64_t sys_monotonic_millis(void) {
    __wasi_timestamp_t timestamp;
    __wasi_errno_t err = __wasi_clock_time_get(
        __WASI_CLOCKID_MONOTONIC,
        1000000, // nanosecond precision
        &timestamp
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        return 0; // Fallback
    }

    return timestamp / 1000000; // Convert ns to ms
}

uint64_t sys_realtime_micros(void) {
    __wasi_timestamp_t timestamp;
    __wasi_errno_t err = __wasi_clock_time_get(
        __WASI_CLOCKID_REALTIME,
        1000, // nanosecond precision
        &timestamp
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        return 0;
    }

    return timestamp / 1000; // Convert ns to μs
}
```

**File: `src/platforms/wasi/memory_wasi.c`**
```c
#include "platform.h"
#include <stddef.h>

// Replace emscripten_resize_heap()
bool sys_resize_memory(size_t new_size_bytes) {
    // Get current memory size
    size_t current_pages = __builtin_wasm_memory_size(0);
    size_t current_bytes = current_pages * 65536; // WASM page = 64KB

    if (new_size_bytes <= current_bytes) {
        return true; // Already sufficient
    }

    // Calculate pages needed
    size_t needed_bytes = new_size_bytes - current_bytes;
    size_t needed_pages = (needed_bytes + 65535) / 65536; // Round up

    // Grow memory
    size_t prev_pages = __builtin_wasm_memory_grow(0, needed_pages);

    return prev_pages != (size_t)-1;
}

size_t sys_get_heap_size(void) {
    size_t pages = __builtin_wasm_memory_size(0);
    return pages * 65536;
}
```

**File: `src/platforms/wasi/random_wasi.c`**
```c
#include "platform.h"
#include <wasi/api.h>

void sys_random_bytes(uint8_t *buffer, size_t len) {
    __wasi_errno_t err = __wasi_random_get(buffer, len);

    if (err != __WASI_ERRNO_SUCCESS) {
        // Fallback: use simple PRNG (not cryptographically secure)
        static uint32_t seed = 12345;
        for (size_t i = 0; i < len; i++) {
            seed = seed * 1103515245 + 12345;
            buffer[i] = (seed >> 16) & 0xFF;
        }
    }
}
```

#### Days 7-9: File I/O Migration

**Challenge**: WASI uses capability-based security model with preopened directories.

**Strategy**:
1. Support embedding AVM files in WASM linear memory (no file I/O needed)
2. Optionally support loading from preopened directories

**File: `src/platforms/wasi/sys_wasi.c`**
```c
#include "platform.h"
#include "module.h"
#include <wasi/api.h>
#include <string.h>

// Option 1: Embed AVM in linear memory
extern const uint8_t _binary_app_avm_start[];
extern const uint8_t _binary_app_avm_end[];

Module *sys_load_embedded_avm(void) {
    size_t size = _binary_app_avm_end - _binary_app_avm_start;
    return module_load_from_memory(_binary_app_avm_start, size);
}

// Option 2: Load from preopened directory
Module *sys_load_avm_from_file(const char *path) {
    __wasi_fd_t fd;

    // Open file from preopened directory (typically fd 3)
    __wasi_errno_t err = __wasi_path_open(
        3,                          // Preopened directory fd
        0,                          // dirflags
        path,                       // path
        strlen(path),               // path_len
        0,                          // oflags (0 = open existing)
        __WASI_RIGHTS_FD_READ |     // fs_rights_base
        __WASI_RIGHTS_FD_SEEK,
        0,                          // fs_rights_inheriting
        0,                          // fdflags
        &fd                         // opened fd
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        return NULL;
    }

    // Get file size
    __wasi_filestat_t stat;
    err = __wasi_fd_filestat_get(fd, &stat);
    if (err != __WASI_ERRNO_SUCCESS) {
        __wasi_fd_close(fd);
        return NULL;
    }

    size_t size = stat.size;

    // Allocate buffer
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        __wasi_fd_close(fd);
        return NULL;
    }

    // Read file
    __wasi_iovec_t iov = {
        .buf = buffer,
        .buf_len = size
    };
    size_t nread;
    err = __wasi_fd_read(fd, &iov, 1, &nread);

    __wasi_fd_close(fd);

    if (err != __WASI_ERRNO_SUCCESS || nread != size) {
        free(buffer);
        return NULL;
    }

    // Load module from buffer
    Module *module = module_load_from_memory(buffer, size);
    free(buffer);

    return module;
}
```

#### Days 10-12: Module Loader Integration

**Modify: `src/libAtomVM/module.c`**

Add platform-specific compilation:

```c
#ifdef AVM_PLATFORM_WASI
    // Use WASI file I/O
    Module *mod = sys_load_avm_from_file(filename);
#elif defined(AVM_PLATFORM_EMSCRIPTEN)
    // Use Emscripten virtual FS
    FILE *fp = fopen(filename, "rb");
    // ... existing code
#else
    // Generic Unix
    int fd = open(filename, O_RDONLY);
    // ... existing code
#endif
```

**Add: `src/libAtomVM/module_memory.c`** (new file)
```c
// Load module from in-memory buffer
Module *module_load_from_memory(const uint8_t *data, size_t size) {
    Module *mod = malloc(sizeof(Module));
    if (!mod) return NULL;

    // Parse AVM format
    // (existing parsing logic adapted for memory buffer)

    return mod;
}
```

---

### Phase 3: WASM Exports & Entry Points (7-10 days)

#### Days 13-14: Export Interface Design

**File: `src/platforms/wasi/exports.c`**
```c
#include "platform.h"
#include "context.h"
#include "module.h"
#include "term.h"
#include <string.h>

// Global context (singleton for simplicity)
static GlobalContext *global_ctx = NULL;
static Module *main_module = NULL;

// Initialize AtomVM
__attribute__((export_name("avm_init")))
int avm_init(void) {
    if (global_ctx != NULL) {
        return 0; // Already initialized
    }

    global_ctx = globalcontext_new();
    if (!global_ctx) {
        return -1;
    }

    // Load embedded or preopened AVM
    main_module = sys_load_embedded_avm();
    if (!main_module) {
        main_module = sys_load_avm_from_file("app.avm");
    }

    if (!main_module) {
        globalcontext_destroy(global_ctx);
        global_ctx = NULL;
        return -2;
    }

    return 0;
}

// Execute function with JSON input/output
__attribute__((export_name("execute")))
uint32_t execute(uint32_t input_ptr, uint32_t input_len) {
    if (!global_ctx || !main_module) {
        return 0; // Not initialized
    }

    // Get input JSON from linear memory
    const char *input_json = (const char *)input_ptr;

    // Create execution context
    Context *ctx = context_new(global_ctx);
    if (!ctx) {
        return 0;
    }

    // Parse JSON to term
    term input_term = term_from_json_string(input_json, input_len, ctx);
    if (term_is_invalid_term(input_term)) {
        context_destroy(ctx);
        return 0;
    }

    // Call main function (e.g., Module:main/1)
    term result_term = context_make_atom(ctx, "main");
    term result = module_call_function(
        main_module,
        ctx,
        result_term,
        1,
        &input_term
    );

    if (term_is_invalid_term(result)) {
        context_destroy(ctx);
        return 0;
    }

    // Convert result to JSON
    char *result_json = term_to_json_string(result, ctx);
    if (!result_json) {
        context_destroy(ctx);
        return 0;
    }

    // Copy to linear memory and return pointer
    size_t result_len = strlen(result_json);
    uint32_t result_ptr = (uint32_t)malloc(result_len + 1);
    memcpy((void *)result_ptr, result_json, result_len + 1);

    free(result_json);
    context_destroy(ctx);

    return result_ptr;
}

// Get result length
__attribute__((export_name("get_result_len")))
uint32_t get_result_len(uint32_t result_ptr) {
    if (result_ptr == 0) return 0;
    return strlen((const char *)result_ptr);
}

// Free result memory
__attribute__((export_name("free_result")))
void free_result(uint32_t result_ptr) {
    if (result_ptr != 0) {
        free((void *)result_ptr);
    }
}

// WASI entry point (optional, for standalone execution)
__attribute__((export_name("_start")))
void _start(void) {
    // Initialize and run
    if (avm_init() != 0) {
        __wasi_proc_exit(1);
    }

    // Could read from stdin or file
    // For now, just exit successfully
    __wasi_proc_exit(0);
}
```

#### Days 15-16: Build System Integration

**Modify: `CMakeLists.txt`** (root)
```cmake
# Add WASI platform option
option(AVM_PLATFORM_WASI "Build for WASI" OFF)

if(AVM_PLATFORM_WASI)
    message(STATUS "Building for WASI platform")

    # Use WASI toolchain
    if(NOT CMAKE_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/wasi-toolchain.cmake)
    endif()

    # Platform-specific flags
    add_compile_definitions(AVM_PLATFORM_WASI)

    # Disable features not supported in WASI
    set(AVM_DISABLE_SMP ON)
    set(AVM_DISABLE_TASK_DRIVER ON)
    set(AVM_DISABLE_NETWORKING ON)

    # Include WASI platform sources
    add_subdirectory(src/platforms/wasi)
endif()
```

**Create: `src/platforms/wasi/CMakeLists.txt`**
```cmake
set(WASI_PLATFORM_SOURCES
    platform.h
    sys_wasi.c
    time_wasi.c
    memory_wasi.c
    random_wasi.c
    exports.c
    main.c
)

add_library(atomvm_platform_wasi STATIC ${WASI_PLATFORM_SOURCES})

target_include_directories(atomvm_platform_wasi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WASI_SDK_PATH}/share/wasi-sysroot/include
)

target_link_libraries(atomvm_platform_wasi
    libAtomVM
)
```

**Build script: `build-wasi.sh`**
```bash
#!/bin/bash
set -e

export WASI_SDK_PATH=/opt/wasi-sdk-22.0

mkdir -p build-wasi
cd build-wasi

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/wasi-toolchain.cmake \
    -DAVM_PLATFORM_WASI=ON \
    -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)

echo "Build complete: build-wasi/src/AtomVM.wasm"
```

#### Days 17-19: Testing with Wasmtime

**Test 1: Standalone Execution**
```bash
# Build test AVM
cd examples/json_validator
mix compile

# Package to AVM format
# (Assuming PackBEAM tool exists or use custom script)
packbeam json_validator.avm \
    _build/dev/lib/json_validator/ebin/*.beam

# Run with wasmtime
wasmtime \
    --dir=. \
    --invoke avm_init \
    ../../build-wasi/src/AtomVM.wasm

# Execute with input
echo '{"data": {"name": "Test"}, "schema": {...}}' > input.json
wasmtime \
    --dir=. \
    --mapdir /::. \
    ../../build-wasi/src/AtomVM.wasm
```

**Test 2: Function Call**
```bash
# Test execute export
wasmtime --invoke execute AtomVM.wasm -- <input_ptr> <input_len>
```

---

### Phase 4: Wasmex Integration (7-10 days)

#### Days 20-22: Elixir/Wasmex Testing

**File: `test_atomvm_wasi.exs`**
```elixir
# Load WASM module
{:ok, bytes} = File.read("build-wasi/src/AtomVM.wasm")
{:ok, module} = Wasmex.Module.from_bytes(bytes)

# Create instance
{:ok, instance} = Wasmex.Instance.new(
  module,
  %{
    # WASI imports
    wasi_snapshot_preview1: %{
      fd_write: {:fn, [:i32, :i32, :i32, :i32], [:i32], fn _, _, _, _, _ -> 0 end},
      fd_read: {:fn, [:i32, :i32, :i32, :i32], [:i32], fn _, _, _, _, _ -> 0 end},
      # ... other WASI functions
    }
  }
)

# Initialize
{:ok, [init_result]} = Wasmex.Instance.call_exported_function(instance, :avm_init, [])
IO.puts("Init result: #{init_result}")

# Prepare input
input_json = Jason.encode!(%{
  "data" => %{"name" => "John", "age" => 30},
  "schema" => %{
    "type" => "object",
    "required" => ["name", "email"]
  }
})

# Write to WASM memory
input_ptr = Wasmex.Memory.alloc(instance.memory, byte_size(input_json))
:ok = Wasmex.Memory.write(instance.memory, input_ptr, input_json)

# Execute
{:ok, [result_ptr]} = Wasmex.Instance.call_exported_function(
  instance,
  :execute,
  [input_ptr, byte_size(input_json)]
)

# Read result
{:ok, [result_len]} = Wasmex.Instance.call_exported_function(
  instance,
  :get_result_len,
  [result_ptr]
)

{:ok, result_json} = Wasmex.Memory.read(instance.memory, result_ptr, result_len)
result = Jason.decode!(result_json)

IO.inspect(result)

# Cleanup
Wasmex.Instance.call_exported_function(instance, :free_result, [result_ptr])
```

#### Days 23-24: Performance Optimization

**Memory Layout Optimization:**
- Minimize allocations in hot paths
- Reuse term buffers where possible
- Profile with wasmtime's profiling tools

**Call Overhead Reduction:**
- Batch operations where possible
- Reduce JSON parsing/encoding overhead
- Consider binary term format instead of JSON

**Module Size Optimization:**
```bash
# Strip debug symbols
wasm-strip AtomVM.wasm

# Optimize with wasm-opt
wasm-opt -O3 AtomVM.wasm -o AtomVM.optimized.wasm

# Check size
ls -lh AtomVM*.wasm
```

#### Days 25-26: Documentation

**Create: `docs/WASI_PLATFORM.md`**
```markdown
# AtomVM WASI Platform

## Overview
The WASI platform enables AtomVM to run in server-side WebAssembly runtimes.

## Building
\`\`\`bash
./build-wasi.sh
\`\`\`

## Usage

### Standalone
\`\`\`bash
wasmtime --dir=. --invoke avm_init AtomVM.wasm
\`\`\`

### With Wasmex (Elixir)
\`\`\`elixir
{:ok, module} = Wasmex.Module.from_file("AtomVM.wasm")
{:ok, instance} = Wasmex.Instance.new(module)
Wasmex.Instance.call(instance, :avm_init, [])
\`\`\`

## Limitations
- No networking (WASI Preview 1)
- No threading
- File I/O via preopened directories only
- No port drivers

## Architecture
See `src/platforms/wasi/README.md` for implementation details.
```

---

## Testing Strategy

### Unit Tests
```c
// tests/wasi/test_memory.c
#include "platform.h"
#include <assert.h>

void test_memory_growth(void) {
    size_t initial = sys_get_heap_size();

    bool success = sys_resize_memory(initial + 65536);
    assert(success);

    size_t after = sys_get_heap_size();
    assert(after >= initial + 65536);
}
```

### Integration Tests
```bash
# Test AVM loading
./test_avm_load.sh

# Test function execution
./test_execute.sh

# Test memory limits
./test_memory_limits.sh
```

### Wasmex Tests
```elixir
defmodule AtomVMWASITest do
  use ExUnit.Case

  test "loads and initializes" do
    {:ok, module} = Wasmex.Module.from_file("AtomVM.wasm")
    {:ok, instance} = Wasmex.Instance.new(module)

    {:ok, [0]} = Wasmex.Instance.call(instance, :avm_init, [])
  end

  test "executes Elixir function" do
    # ... test execution
  end
end
```

---

## Deliverables

### Code
- [ ] Platform abstraction layer (`src/platforms/wasi/`)
- [ ] Modified build system (CMake)
- [ ] WASM exports interface
- [ ] Memory management for WASI
- [ ] File I/O using WASI APIs

### Documentation
- [ ] Build instructions
- [ ] Platform limitations
- [ ] API documentation
- [ ] Integration examples

### Tests
- [ ] Unit tests for platform layer
- [ ] Integration tests with wasmtime
- [ ] Wasmex integration tests
- [ ] Performance benchmarks

---

## Risks & Mitigation

### Risk 1: Incomplete Emscripten Mapping
**Mitigation**: Incremental approach - disable unsupported features, document limitations

### Risk 2: WASI Capability Model Complexity
**Mitigation**: Start with embedded AVM (no file I/O), add file support later

### Risk 3: Performance Overhead
**Mitigation**: Profile early, optimize hot paths, consider ahead-of-time compilation

### Risk 4: Upstream Acceptance
**Mitigation**: Engage with AtomVM maintainers early, follow contribution guidelines

---

## Success Criteria

### MVP (Minimum Viable Product)
- ✅ AtomVM compiles to WASI-compatible WASM
- ✅ Can load and execute single embedded AVM module
- ✅ JSON input/output works
- ✅ Runs in wasmtime standalone
- ✅ Integrates with Wasmex from Elixir

### Full Release
- ✅ Support for preopened file I/O
- ✅ Multiple modules/dependencies
- ✅ Comprehensive test suite
- ✅ Documentation complete
- ✅ Performance acceptable (< 2x overhead vs native)
- ✅ Merged to upstream AtomVM

---

## Timeline Summary

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| Setup & Research | 3-4 days | Design doc, toolchain |
| Platform Layer | 10-14 days | WASI platform code |
| WASM Exports | 7-10 days | Build system, exports |
| Integration | 7-10 days | Wasmex tests, docs |
| **Total** | **27-38 days** | Production-ready WASI support |

---

## Alternative: MVP Fast Track (2-3 weeks)

If speed is critical, build a minimal executor:

### Scope
- Single embedded AVM only
- No file I/O
- JSON input/output only
- No NIFs or ports

### Changes
- Skip complex file I/O
- Hardcode embedded module
- Simpler build system

### Tradeoff
- ✅ Faster to market (15-20 days)
- ❌ Less general-purpose
- ❌ Harder to extend later

---

## Resources

### Documentation
- WASI Specification: https://github.com/WebAssembly/WASI
- AtomVM Repository: https://github.com/atomvm/AtomVM
- Wasmtime Book: https://docs.wasmtime.dev/
- wasi-libc: https://github.com/WebAssembly/wasi-libc

### Tools
- WASI SDK: https://github.com/WebAssembly/wasi-sdk
- wasmtime: https://wasmtime.dev/
- wasm-opt: https://github.com/WebAssembly/binaryen
- Wasmex: https://hex.pm/packages/wasmex

### Community
- AtomVM Discussions: https://github.com/atomvm/AtomVM/discussions
- WASI on Slack: https://wasi.dev/
- Elixir Forum: https://elixirforum.com/

---

## Next Steps

1. **Engage with AtomVM maintainers**
   - Open GitHub issue describing WASI support proposal
   - Get feedback on architecture approach
   - Coordinate to avoid duplicate work

2. **Set up development environment**
   - Install WASI SDK
   - Build AtomVM from source
   - Verify current Emscripten build works

3. **Create proof of concept**
   - Minimal "hello world" with WASI
   - Single exported function
   - Validate toolchain works

4. **Iterate incrementally**
   - Add features one at a time
   - Test frequently with wasmtime
   - Document as you go

---

## Conclusion

Adding WASI support to AtomVM is **technically feasible** with **25-40 days of focused effort**. The work breaks down into clear phases with concrete deliverables.

**Recommended approach**: Full WASI port (Approach 1) for long-term sustainability and upstream contribution potential.

**Quick win**: Collaborate with AtomVM maintainers who have already identified WASI as needed - share the effort and benefit the entire community.

This is a **valuable contribution** that would enable:
- Server-side Elixir in WASM (via Wasmex)
- Edge computing with Elixir
- Sandboxed plugin systems
- Portable Elixir executables

Good luck with the implementation! 🚀
