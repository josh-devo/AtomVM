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

#include <sys.h>

#include "platform_wasi.h"

#include <avmpack.h>
#include <context.h>
#include <defaultatoms.h>
#include <globalcontext.h>
#include <iff.h>
#include <module.h>
#include <scheduler.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wasi/api.h>

#include <trace.h>

// Platform initialization
void sys_init_platform(GlobalContext *glb)
{
    TRACE("sys_init_platform: Initializing WASI platform\n");

    struct WASIPlatformData *platform = malloc(sizeof(struct WASIPlatformData));
    if (!platform) {
        fprintf(stderr, "sys_init_platform: Cannot allocate platform data\n");
        AVM_ABORT();
    }

    // Initialize random number generation
    platform->entropy_is_initialized = false;
    platform->random_is_initialized = false;

    // Initialize preopened directories
    platform->has_preopen = false;
    platform->preopen_fd = 3; // Standard fd 3 is typically the first preopen
    wasi_init_preopens(platform);

    glb->platform_data = platform;

    TRACE("sys_init_platform: WASI platform initialized successfully\n");
}

void sys_free_platform(GlobalContext *glb)
{
    TRACE("sys_free_platform: Freeing WASI platform\n");

    struct WASIPlatformData *platform = glb->platform_data;

    if (platform->random_is_initialized) {
        mbedtls_ctr_drbg_free(&platform->random_ctx);
    }

    if (platform->entropy_is_initialized) {
        mbedtls_entropy_free(&platform->entropy_ctx);
    }

    free(platform);
    glb->platform_data = NULL;

    TRACE("sys_free_platform: WASI platform freed\n");
}

// Event polling (stub for WASI - no async events)
void sys_poll_events(GlobalContext *glb, int timeout_ms)
{
    (void)glb;
    (void)timeout_ms;
    // WASI has no async event system, this is a no-op
}

// Listener management (not applicable for WASI)
void sys_listener_destroy(struct ListHead *item)
{
    (void)item;
}

void sys_register_select_event(GlobalContext *global, ErlNifEvent event, bool is_write)
{
    (void)global;
    (void)event;
    (void)is_write;
}

void sys_unregister_select_event(GlobalContext *global, ErlNifEvent event, bool is_write)
{
    (void)global;
    (void)event;
    (void)is_write;
}

// Time functions
void sys_time(struct timespec *t)
{
    wasi_time(t);
}

void sys_monotonic_time(struct timespec *t)
{
    wasi_monotonic_time(t);
}

uint64_t sys_monotonic_time_u64(void)
{
    // AtomVM expects nanoseconds
    return wasi_monotonic_time_ns();
}

uint64_t sys_monotonic_time_ms_to_u64(uint64_t ms)
{
    // Convert milliseconds to nanoseconds
    return ms * 1000000ULL;
}

uint64_t sys_monotonic_time_u64_to_ms(uint64_t t)
{
    // Convert nanoseconds to milliseconds
    return t / 1000000ULL;
}

// File I/O functions
void wasi_init_preopens(struct WASIPlatformData *platform)
{
    // Try to check if fd 3 is valid (typical first preopen)
    __wasi_fdstat_t fdstat;
    __wasi_errno_t err = __wasi_fd_fdstat_get(3, &fdstat);

    if (err == __WASI_ERRNO_SUCCESS) {
        platform->has_preopen = true;
        platform->preopen_fd = 3;
        TRACE("wasi_init_preopens: Found preopened directory at fd 3\n");
    } else {
        platform->has_preopen = false;
        TRACE("wasi_init_preopens: No preopened directories found\n");
    }
}

bool wasi_file_exists(const char *path)
{
    if (!path) {
        return false;
    }

    // Try to get file stats
    __wasi_fd_t fd = 3; // Default preopen
    __wasi_filestat_t filestat;

    __wasi_errno_t err = __wasi_path_filestat_get(
        fd,
        0, // flags
        path,
        strlen(path),
        &filestat
    );

    return err == __WASI_ERRNO_SUCCESS;
}

void *wasi_read_file(const char *path, size_t *size_out)
{
    if (!path || !size_out) {
        return NULL;
    }

    TRACE("wasi_read_file: Reading file %s\n", path);

    __wasi_fd_t fd;

    // Open file from preopened directory
    __wasi_errno_t err = __wasi_path_open(
        3, // Preopened directory fd
        0, // dirflags
        path,
        strlen(path),
        0, // oflags (0 = open existing)
        __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK | __WASI_RIGHTS_FD_FILESTAT_GET,
        0, // fs_rights_inheriting
        0, // fdflags
        &fd
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        TRACE("wasi_read_file: Failed to open %s, errno=%d\n", path, err);
        return NULL;
    }

    // Get file size
    __wasi_filestat_t stat;
    err = __wasi_fd_filestat_get(fd, &stat);
    if (err != __WASI_ERRNO_SUCCESS) {
        TRACE("wasi_read_file: Failed to get file stats, errno=%d\n", err);
        __wasi_fd_close(fd);
        return NULL;
    }

    size_t size = stat.size;
    *size_out = size;

    // Allocate buffer
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        fprintf(stderr, "wasi_read_file: Failed to allocate %zu bytes\n", size);
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
        TRACE("wasi_read_file: Failed to read file, errno=%d, read=%zu/%zu\n", err, nread, size);
        free(buffer);
        return NULL;
    }

    TRACE("wasi_read_file: Successfully read %zu bytes from %s\n", size, path);

    return buffer;
}

// Module loading
enum OpenAVMResult sys_open_avm_from_file(
    GlobalContext *global, const char *path, struct AVMPackData **avm_data)
{
    TRACE("sys_open_avm_from_file: Opening AVM from %s\n", path);

    (void)global;

    size_t size;
    void *data = wasi_read_file(path, &size);
    if (!data) {
        fprintf(stderr, "sys_open_avm_from_file: Cannot read %s\n", path);
        return AVM_OPEN_CANNOT_OPEN;
    }

    struct ConstAVMPack *const_avm = malloc(sizeof(struct ConstAVMPack));
    if (!const_avm) {
        free(data);
        return AVM_OPEN_FAILED_ALLOC;
    }

    avmpack_data_init(&const_avm->base, &const_avm_pack_info);
    const_avm->base.data = (const uint8_t *)data;

    *avm_data = &const_avm->base;

    TRACE("sys_open_avm_from_file: Successfully opened AVM from %s\n", path);

    return AVM_OPEN_OK;
}

Module *sys_load_module_from_file(GlobalContext *global, const char *path)
{
    TRACE("sys_load_module_from_file: Loading module from %s\n", path);

    size_t size;
    void *data = wasi_read_file(path, &size);
    if (!data) {
        fprintf(stderr, "sys_load_module_from_file: Cannot read %s\n", path);
        return NULL;
    }

    if (!iff_is_valid_beam(data)) {
        fprintf(stderr, "sys_load_module_from_file: %s is not a valid BEAM file\n", path);
        free(data);
        return NULL;
    }

    Module *new_module = module_new_from_iff_binary(global, data, size);
    if (!new_module) {
        free(data);
        return NULL;
    }

    new_module->module_platform_data = NULL;

    TRACE("sys_load_module_from_file: Successfully loaded module from %s\n", path);

    return new_module;
}

// Port creation (not supported in WASI)
Context *sys_create_port(GlobalContext *glb, const char *driver_name, term opts)
{
    (void)glb;
    (void)driver_name;
    (void)opts;

    TRACE("sys_create_port: Ports not supported in WASI platform\n");

    return NULL;
}

// System info
term sys_get_info(Context *ctx, term key)
{
    (void)ctx;
    (void)key;

    // Could return platform-specific info here
    return UNDEFINED_ATOM;
}

// mbedtls integration for random/crypto
int sys_mbedtls_entropy_func(void *entropy, unsigned char *buf, size_t size)
{
    return mbedtls_entropy_func(entropy, buf, size);
}

mbedtls_entropy_context *sys_mbedtls_get_entropy_context_lock(GlobalContext *global)
{
    struct WASIPlatformData *platform = global->platform_data;

    if (!platform->entropy_is_initialized) {
        mbedtls_entropy_init(&platform->entropy_ctx);

        // Add WASI random as entropy source
        mbedtls_entropy_add_source(&platform->entropy_ctx,
            wasi_mbedtls_entropy_source,
            NULL,
            MBEDTLS_ENTROPY_MIN_PLATFORM,
            MBEDTLS_ENTROPY_SOURCE_STRONG);

        platform->entropy_is_initialized = true;

        TRACE("sys_mbedtls_get_entropy_context_lock: Initialized entropy context\n");
    }

    return &platform->entropy_ctx;
}

void sys_mbedtls_entropy_context_unlock(GlobalContext *global)
{
    (void)global;
    // No locking needed in single-threaded WASI
}

mbedtls_ctr_drbg_context *sys_mbedtls_get_ctr_drbg_context_lock(GlobalContext *global)
{
    struct WASIPlatformData *platform = global->platform_data;

    if (!platform->random_is_initialized) {
        mbedtls_ctr_drbg_init(&platform->random_ctx);

        mbedtls_entropy_context *entropy_ctx = sys_mbedtls_get_entropy_context_lock(global);
        sys_mbedtls_entropy_context_unlock(global);

        const char *seed = "AtomVM WASI Mbed-TLS initial seed.";
        int seed_len = strlen(seed);
        int seed_err = mbedtls_ctr_drbg_seed(&platform->random_ctx, sys_mbedtls_entropy_func,
            entropy_ctx, (const unsigned char *)seed, seed_len);

        if (seed_err != 0) {
            fprintf(stderr, "sys_mbedtls_get_ctr_drbg_context_lock: Failed to seed RNG\n");
            abort();
        }

        platform->random_is_initialized = true;

        TRACE("sys_mbedtls_get_ctr_drbg_context_lock: Initialized DRBG context\n");
    }

    return &platform->random_ctx;
}

void sys_mbedtls_ctr_drbg_context_unlock(GlobalContext *global)
{
    (void)global;
    // No locking needed in single-threaded WASI
}
