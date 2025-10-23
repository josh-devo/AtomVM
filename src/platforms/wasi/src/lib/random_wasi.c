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
#include <wasi/api.h>

#include <trace.h>

int wasi_random_bytes(uint8_t *buffer, size_t len)
{
    if (!buffer || len == 0) {
        return -1;
    }

    __wasi_errno_t err = __wasi_random_get(buffer, len);

    if (err != __WASI_ERRNO_SUCCESS) {
        fprintf(stderr, "wasi_random_bytes: random_get failed with errno %d\n", err);
        return -1;
    }

    TRACE("wasi_random_bytes: generated %zu random bytes\n", len);

    return 0;
}

int wasi_mbedtls_entropy_source(void *data, unsigned char *output, size_t len, size_t *olen)
{
    (void)data; // Unused parameter

    if (!output || !olen) {
        return -1;
    }

    __wasi_errno_t err = __wasi_random_get(output, len);

    if (err != __WASI_ERRNO_SUCCESS) {
        fprintf(stderr, "wasi_mbedtls_entropy_source: random_get failed with errno %d\n", err);
        *olen = 0;
        return -1;
    }

    *olen = len;

    TRACE("wasi_mbedtls_entropy_source: provided %zu bytes of entropy\n", len);

    return 0;
}
