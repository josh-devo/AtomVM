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

#ifndef _PLATFORM_WASI_H_
#define _PLATFORM_WASI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <wasi/api.h>

#ifdef HAVE_MBEDTLS
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#if defined(MBEDTLS_VERSION_NUMBER) && (MBEDTLS_VERSION_NUMBER >= 0x03000000)
#include <mbedtls/build_info.h>
#else
#include <mbedtls/config.h>
#endif

#include "sys_mbedtls.h"
#endif

// Platform capabilities
#define PLATFORM_HAS_FILESYSTEM 1
#define PLATFORM_HAS_NETWORKING 0
#define PLATFORM_HAS_THREADING 0
#define PLATFORM_HAS_SOCKETS 0

// Platform-specific types
typedef __wasi_fd_t avm_file_t;
typedef __wasi_timestamp_t avm_timestamp_t;

/**
 * @brief Platform-specific data for WASI
 * @details This structure contains all WASI-specific runtime data
 */
struct WASIPlatformData
{
#ifdef HAVE_MBEDTLS
    // Random number generation (mbedtls)
    mbedtls_entropy_context entropy_ctx;
    bool entropy_is_initialized;
    mbedtls_ctr_drbg_context random_ctx;
    bool random_is_initialized;
#endif

    // Preopened directories (for file I/O)
    __wasi_fd_t preopen_fd;
    bool has_preopen;
};

// Forward declarations
struct GlobalContext;

/**
 * @brief Initialize WASI platform data
 * @param glb Global context
 */
void wasi_platform_init(struct GlobalContext *glb);

/**
 * @brief Free WASI platform data
 * @param glb Global context
 */
void wasi_platform_free(struct GlobalContext *glb);

// Time functions
/**
 * @brief Get monotonic time in microseconds
 * @return Monotonic time in microseconds since an arbitrary point
 */
uint64_t wasi_monotonic_time_us(void);

/**
 * @brief Get monotonic time in nanoseconds
 * @return Monotonic time in nanoseconds since an arbitrary point
 */
uint64_t wasi_monotonic_time_ns(void);

/**
 * @brief Get wall clock time
 * @param t Pointer to timespec structure to fill
 */
void wasi_time(struct timespec *t);

/**
 * @brief Get monotonic time
 * @param t Pointer to timespec structure to fill
 */
void wasi_monotonic_time(struct timespec *t);

// Memory functions
/**
 * @brief Resize WebAssembly linear memory
 * @param new_size_bytes Requested new size in bytes
 * @return true if successful, false otherwise
 */
bool wasi_resize_memory(size_t new_size_bytes);

/**
 * @brief Get current heap size
 * @return Current heap size in bytes
 */
size_t wasi_get_heap_size(void);

// Random functions
/**
 * @brief Get random bytes
 * @param buffer Buffer to fill with random data
 * @param len Number of bytes to generate
 * @return 0 on success, non-zero on error
 */
int wasi_random_bytes(uint8_t *buffer, size_t len);

/**
 * @brief WASI entropy source for mbedtls
 * @param data Unused (for compatibility)
 * @param output Buffer to fill with random data
 * @param len Number of bytes to generate
 * @param olen Pointer to store actual number of bytes generated
 * @return 0 on success, non-zero on error
 */
int wasi_mbedtls_entropy_source(void *data, unsigned char *output, size_t len, size_t *olen);

// File I/O functions
/**
 * @brief Check if file exists in preopened directory
 * @param path Path to file (relative to preopen)
 * @return true if file exists, false otherwise
 */
bool wasi_file_exists(const char *path);

/**
 * @brief Read entire file into memory
 * @param path Path to file (relative to preopen)
 * @param size_out Pointer to store file size
 * @return Pointer to allocated buffer with file contents, or NULL on error
 *         Caller must free() the returned buffer
 */
void *wasi_read_file(const char *path, size_t *size_out);

/**
 * @brief Initialize preopened directories
 * @param platform Platform data structure
 */
void wasi_init_preopens(struct WASIPlatformData *platform);

#endif // _PLATFORM_WASI_H_
