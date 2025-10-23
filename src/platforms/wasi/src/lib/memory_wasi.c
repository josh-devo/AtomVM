/*
 * This file is part of AtomVM.
 *
 * Copyright 2025 AtomVM Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
 */

#include "platform_wasi.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <trace.h>

// WebAssembly page size is always 64KB
#define WASM_PAGE_SIZE 65536

bool wasi_resize_memory(size_t new_size_bytes)
{
    // Get current memory size in pages
    size_t current_pages = __builtin_wasm_memory_size(0);
    size_t current_bytes = current_pages * WASM_PAGE_SIZE;

    TRACE("wasi_resize_memory: current=%zu bytes (%zu pages), requested=%zu bytes\n",
          current_bytes, current_pages, new_size_bytes);

    // Check if we already have enough memory
    if (new_size_bytes <= current_bytes) {
        TRACE("wasi_resize_memory: sufficient memory already available\n");
        return true;
    }

    // Calculate how many additional pages we need
    size_t needed_bytes = new_size_bytes - current_bytes;
    size_t needed_pages = (needed_bytes + WASM_PAGE_SIZE - 1) / WASM_PAGE_SIZE; // Round up

    TRACE("wasi_resize_memory: growing by %zu pages\n", needed_pages);

    // Grow memory
    size_t prev_pages = __builtin_wasm_memory_grow(0, needed_pages);

    if (prev_pages == (size_t)-1) {
        fprintf(stderr, "wasi_resize_memory: failed to grow memory by %zu pages\n", needed_pages);
        return false;
    }

    TRACE("wasi_resize_memory: successfully grew memory from %zu to %zu pages\n",
          prev_pages, prev_pages + needed_pages);

    return true;
}

size_t wasi_get_heap_size(void)
{
    size_t pages = __builtin_wasm_memory_size(0);
    size_t bytes = pages * WASM_PAGE_SIZE;

    TRACE("wasi_get_heap_size: %zu bytes (%zu pages)\n", bytes, pages);

    return bytes;
}
