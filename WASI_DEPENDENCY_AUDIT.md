# AtomVM Emscripten to WASI Dependency Audit

**Date**: 2025-10-23
**Purpose**: Map all Emscripten-specific dependencies to WASI equivalents for WASI platform implementation

---

## Executive Summary

AtomVM's Emscripten platform uses several Emscripten-specific APIs that need to be replaced with WASI equivalents:

### Critical Dependencies
1. **Timing**: `emscripten_get_now()` → WASI `clock_time_get()`
2. **File I/O**: `emscripten_fetch()` / VFS → WASI `fd_*` APIs
3. **Threading**: `emscripten_dispatch_to_thread()`, `pthread` → Limited WASI support
4. **Promises**: `emscripten_promise_*` → Not applicable for WASI
5. **HTML Events**: Emscripten HTML5 APIs → Not applicable for WASI
6. **WebSockets**: Emscripten WebSocket APIs → Not applicable for WASI

### Compatibility Matrix

| Feature | Emscripten | WASI | Migration Strategy |
|---------|------------|------|-------------------|
| Timing | `emscripten_get_now()` | `clock_time_get()` | Direct replacement |
| File I/O (read) | `emscripten_fetch()`, `open()` | `fd_read()`, `path_open()` | Use capability model |
| Random | `getentropy()` (works) | `random_get()` | Direct replacement |
| Memory | Handled by Emscripten | `memory.grow` | Use `__builtin_wasm_memory_grow` |
| Threading | `pthread`, `emscripten_dispatch_to_thread()` | ❌ Not in WASI Preview 1 | Disable SMP |
| Promises | `emscripten_promise_*` | ❌ Not applicable | Remove/stub |
| HTML Events | `emscripten_set_*_callback()` | ❌ Not applicable | Remove/stub |
| WebSockets | `emscripten_websocket_*` | ❌ Not in WASI Preview 1 | Remove/stub |
| Network | Limited via fetch | ❌ Not in WASI Preview 1 | Remove/stub |

---

## Detailed Analysis

### 1. Timing Functions

#### Emscripten Implementation
**File**: `src/platforms/emscripten/src/lib/sys.c:618-620`

```c
uint64_t sys_monotonic_time_u64()
{
    double now = emscripten_get_now() * 1000000.0;
    return (uint64_t) now;
}
```

**Dependencies**:
- `emscripten_get_now()` - Returns time in milliseconds as double
- Also uses standard `clock_gettime()` which is POSIX-compatible

#### WASI Replacement

```c
#include <wasi/api.h>

uint64_t sys_monotonic_time_u64()
{
    __wasi_timestamp_t timestamp;
    __wasi_errno_t err = __wasi_clock_time_get(
        __WASI_CLOCKID_MONOTONIC,
        1000,  // nanosecond precision
        &timestamp
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        return 0;  // Fallback
    }

    return timestamp;  // Already in nanoseconds
}
```

**Status**: ✅ Easy - Direct replacement available

---

### 2. File I/O

#### Emscripten Implementation
**File**: `src/platforms/emscripten/src/lib/sys.c:632-683`

```c
static emscripten_fetch_t *fetch_file(const char *url)
{
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    emscripten_fetch_t *fetch = emscripten_fetch(&attr, url);
    // ...
}

static void *load_or_fetch_file(const char *path, emscripten_fetch_t **fetch, size_t *size)
{
    // Tries open() first, falls back to emscripten_fetch()
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        // Use standard POSIX file I/O
    } else if (errno == ENOENT) {
        // Fetch via HTTP
        *fetch = fetch_file(path);
    }
}
```

**Dependencies**:
- `emscripten_fetch()` - HTTP fetch (not needed for WASI)
- Standard POSIX `open()`, `read()`, `lseek()`, `close()` - These map to WASI

#### WASI Replacement

**Strategy**:
1. **Option A**: Embed AVM files in WASM linear memory (no file I/O)
2. **Option B**: Use WASI preopened directories for file access

```c
#include <wasi/api.h>

// Option A: Embedded AVM in linear memory
extern const uint8_t _binary_app_avm_start[];
extern const uint8_t _binary_app_avm_end[];

Module *sys_load_embedded_avm(void) {
    size_t size = _binary_app_avm_end - _binary_app_avm_start;
    return module_load_from_memory(_binary_app_avm_start, size);
}

// Option B: Load from preopened directory
enum OpenAVMResult sys_open_avm_from_file(
    GlobalContext *global, const char *path, struct AVMPackData **avm_data)
{
    __wasi_fd_t fd;

    // Open file from preopened directory (fd 3 by convention)
    __wasi_errno_t err = __wasi_path_open(
        3,                          // Preopened directory fd
        0,                          // dirflags
        path,                       // path
        strlen(path),               // path_len
        0,                          // oflags
        __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK,
        0,                          // fs_rights_inheriting
        0,                          // fdflags
        &fd
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        return AVM_OPEN_CANNOT_OPEN;
    }

    // Get file size
    __wasi_filestat_t stat;
    err = __wasi_fd_filestat_get(fd, &stat);
    if (err != __WASI_ERRNO_SUCCESS) {
        __wasi_fd_close(fd);
        return AVM_OPEN_CANNOT_OPEN;
    }

    size_t size = stat.size;
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        __wasi_fd_close(fd);
        return AVM_OPEN_FAILED_ALLOC;
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
        return AVM_OPEN_CANNOT_OPEN;
    }

    // Parse AVM from buffer
    struct ConstAVMPack *const_avm = malloc(sizeof(struct ConstAVMPack));
    if (!const_avm) {
        free(buffer);
        return AVM_OPEN_FAILED_ALLOC;
    }
    avmpack_data_init(&const_avm->base, &const_avm_pack_info);
    const_avm->base.data = buffer;

    *avm_data = &const_avm->base;
    return AVM_OPEN_OK;
}
```

**Status**: ⚠️ Medium - Requires refactoring file loading logic

---

### 3. Random Number Generation

#### Emscripten Implementation
Uses mbedtls with entropy sources. The actual random generation uses standard `mbedtls_ctr_drbg_*` which should work with WASI.

**File**: `src/platforms/emscripten/src/lib/sys.c:763-828`

Currently uses mbedtls entropy/DRBG which works with POSIX `getentropy()`.

#### WASI Replacement

```c
#include <wasi/api.h>

void sys_get_random_bytes(uint8_t *buffer, size_t len)
{
    __wasi_errno_t err = __wasi_random_get(buffer, len);

    if (err != __WASI_ERRNO_SUCCESS) {
        // Fallback: simple PRNG (not cryptographically secure)
        static uint32_t seed = 12345;
        for (size_t i = 0; i < len; i++) {
            seed = seed * 1103515245 + 12345;
            buffer[i] = (seed >> 16) & 0xFF;
        }
    }
}
```

**Alternative**: Keep mbedtls and add WASI random as entropy source:

```c
int sys_mbedtls_entropy_wasi_source(void *data, unsigned char *output, size_t len, size_t *olen)
{
    __wasi_errno_t err = __wasi_random_get(output, len);
    if (err == __WASI_ERRNO_SUCCESS) {
        *olen = len;
        return 0;
    }
    return -1;
}

// In init:
mbedtls_entropy_add_source(&platform->entropy_ctx,
    sys_mbedtls_entropy_wasi_source,
    NULL,
    MBEDTLS_ENTROPY_MIN_PLATFORM,
    MBEDTLS_ENTROPY_SOURCE_STRONG);
```

**Status**: ✅ Easy - Direct API available

---

### 4. Memory Management

#### Emscripten Implementation
Emscripten handles memory growth automatically or via `emscripten_resize_heap()`.

AtomVM doesn't directly call `emscripten_resize_heap()` in the code I reviewed, but it's likely used implicitly through malloc/realloc.

#### WASI Replacement

```c
#include <stddef.h>

bool sys_resize_memory(size_t new_size_bytes)
{
    // Get current memory size
    size_t current_pages = __builtin_wasm_memory_size(0);
    size_t current_bytes = current_pages * 65536;  // WASM page = 64KB

    if (new_size_bytes <= current_bytes) {
        return true;  // Already sufficient
    }

    // Calculate pages needed
    size_t needed_bytes = new_size_bytes - current_bytes;
    size_t needed_pages = (needed_bytes + 65535) / 65536;  // Round up

    // Grow memory
    size_t prev_pages = __builtin_wasm_memory_grow(0, needed_pages);

    return prev_pages != (size_t)-1;
}

size_t sys_get_heap_size(void)
{
    size_t pages = __builtin_wasm_memory_size(0);
    return pages * 65536;
}
```

**Status**: ✅ Easy - Builtin WebAssembly instructions available

---

### 5. Threading (SMP)

#### Emscripten Implementation
**File**: `src/platforms/emscripten/src/lib/sys.c`

Uses:
- `pthread_mutex_*`, `pthread_cond_*` - Standard POSIX threading
- `emscripten_dispatch_to_thread()` - Emscripten-specific for main thread dispatch
- `emscripten_main_runtime_thread_id()` - Get main thread ID

```c
void sys_signal(GlobalContext *glb)
{
    struct EmscriptenMessageBase *message = malloc(sizeof(struct EmscriptenMessageBase));
    message->message_type = Signal;
    sys_enqueue_emscripten_message(glb, message);
}
```

#### WASI Replacement

**Strategy**: Disable SMP for MVP

WASI Preview 1 does not support threading. WASI Preview 2 and wasi-threads proposal add thread support, but they're not widely available yet.

For MVP:
```cmake
set(AVM_DISABLE_SMP ON)
```

All `#ifndef AVM_NO_SMP` code will be compiled out.

**Status**: ❌ Not available - Disable for MVP, future WASI versions may support

---

### 6. Promises & JavaScript Interop

#### Emscripten Implementation
**File**: `src/platforms/emscripten/src/lib/sys.c:58-95, 248-274`

```c
void sys_promise_resolve_int_and_destroy(em_promise_t promise, em_promise_result_t result, int value)
{
    if (result == EM_PROMISE_FULFILL) {
        EM_ASM({
            promiseMap.get($0).resolve($1);
        }, promise, value);
    }
    emscripten_promise_destroy(promise);
}

em_promise_t sys_enqueue_emscripten_call_message(GlobalContext *glb, const char *target, const char *message)
{
    em_promise_t promise = emscripten_promise_create();
    // ... enqueue message with promise
    return promise;
}
```

#### WASI Replacement

**Strategy**: Not applicable - WASI has no JavaScript runtime

For WASI, we'll implement a simpler request/response model:
1. **Synchronous calls**: Function calls return results directly
2. **Message passing**: Use stdin/stdout or exported functions for I/O
3. **No promises**: Remove promise-based APIs

**Status**: ❌ Not applicable - Design new API without promises

---

### 7. HTML Events

#### Emscripten Implementation
**File**: `src/platforms/emscripten/src/lib/sys.c:316-434`

Extensive use of Emscripten HTML5 event APIs:
- `emscripten_set_keypress_callback_on_thread()`
- `emscripten_set_click_callback_on_thread()`
- `emscripten_set_mousedown_callback_on_thread()`
- etc.

#### WASI Replacement

**Strategy**: Not applicable - WASI has no browser/DOM access

Remove all HTML event handlers for WASI platform.

**Status**: ❌ Not applicable - Remove entirely

---

### 8. WebSockets

#### Emscripten Implementation
**File**: `src/platforms/emscripten/src/lib/websocket_nifs.c`

Uses Emscripten WebSocket API for browser WebSocket support.

#### WASI Replacement

**Strategy**: Not applicable - No networking in WASI Preview 1

WASI Preview 2 may add networking via wasi-sockets proposal, but it's not available yet.

**Status**: ❌ Not available in WASI Preview 1 - Remove for MVP

---

## Files Requiring Modification

### Core Platform Files (New)

Create `src/platforms/wasi/` directory:

```
src/platforms/wasi/
├── CMakeLists.txt          # Build configuration
├── platform.h              # Platform-specific types and capabilities
├── sys_wasi.c              # System calls and file I/O
├── memory_wasi.c           # Memory management
├── time_wasi.c             # Timing functions
├── random_wasi.c           # Random number generation
├── exports.c               # WASM export interface
└── main.c                  # Entry point
```

### Existing Files to Modify

1. **`src/libAtomVM/module.c`**
   - Add `#ifdef AVM_PLATFORM_WASI` sections for WASI file loading
   - Abstract file I/O to support embedded AVMs

2. **`CMakeLists.txt`** (root)
   - Add `AVM_PLATFORM_WASI` option
   - Include WASI platform subdirectory

3. **`src/libAtomVM/globalcontext.c`**
   - May need minor adjustments for WASI platform data

### Files NOT Needed for WASI

These Emscripten-specific files have no WASI equivalent:
- `src/platforms/emscripten/src/lib/websocket_nifs.c`
- `src/platforms/emscripten/src/lib/platform_nifs.c` (HTML events)
- `src/platforms/emscripten/src/atomvm.pre.js` (JavaScript wrapper)

---

## Implementation Plan

### Phase 1: Basic Platform (Days 1-6)

✅ **Day 1**: Install WASI SDK
✅ **Day 2**: Audit dependencies (this document)
- **Day 3**: Create platform abstraction layer structure
- **Day 4**: Implement timing functions (`time_wasi.c`)
- **Day 5**: Implement memory management (`memory_wasi.c`)
- **Day 6**: Implement random number generation (`random_wasi.c`)

### Phase 2: File I/O (Days 7-12)

- **Day 7-8**: Design module loading strategy (embedded vs file-based)
- **Day 9-10**: Implement `sys_wasi.c` with WASI file I/O
- **Day 11-12**: Add `module_load_from_memory()` support

### Phase 3: Build & Export (Days 13-19)

- **Day 13-14**: Create CMake build system integration
- **Day 15-16**: Design and implement WASM export interface
- **Day 17-19**: Test with wasmtime

### Phase 4: Integration (Days 20-26)

- **Day 20-22**: Test with Wasmex from Elixir
- **Day 23-24**: Performance optimization
- **Day 25-26**: Documentation and tests

---

## Feature Compatibility

### Supported in WASI MVP

- ✅ File I/O (via preopened directories)
- ✅ Standard I/O (stdin/stdout/stderr)
- ✅ Environment variables
- ✅ Random numbers (`random_get`)
- ✅ Clock/time functions
- ✅ Memory growth
- ✅ Basic BEAM execution

### NOT Supported in WASI MVP

- ❌ Threading/SMP
- ❌ Networking (no sockets/HTTP)
- ❌ WebSockets
- ❌ HTML events
- ❌ JavaScript promises
- ❌ Port drivers (mostly)

---

## Testing Strategy

### Unit Tests
- Test timing functions
- Test memory growth
- Test random generation
- Test file loading (embedded and preopened)

### Integration Tests
- Load and execute simple BEAM modules
- Test with wasmtime standalone
- Test with Wasmex from Elixir

### Performance Tests
- Measure WASM binary size
- Measure execution speed vs Emscripten
- Memory usage analysis

---

## Success Criteria

### MVP (Minimum Viable Product)

1. ✅ AtomVM compiles to WASI-compatible WASM
2. ✅ Can load embedded AVM module from linear memory
3. ✅ Can execute BEAM bytecode
4. ✅ Basic file I/O works (preopened directories)
5. ✅ Runs in wasmtime standalone
6. ✅ Can be called from Elixir via Wasmex

### Limitations Documented

- No threading (single-threaded only)
- No networking
- No browser-specific features
- File I/O requires preopening directories

---

## Conclusion

The Emscripten → WASI port is feasible with these key changes:

1. **Replace timing**: `emscripten_get_now()` → `clock_time_get()`
2. **Replace file I/O**: `emscripten_fetch()` → `fd_*` APIs or embedded AVMs
3. **Replace random**: Use `random_get()`
4. **Replace memory**: Use `__builtin_wasm_memory_grow()`
5. **Remove threading**: Compile with `AVM_NO_SMP`
6. **Remove browser features**: No promises, HTML events, WebSockets

Estimated effort: **25-30 days** for a working MVP, **40+ days** for production-ready with tests and documentation.
