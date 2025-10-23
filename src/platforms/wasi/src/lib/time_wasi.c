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

#include <stdio.h>
#include <time.h>
#include <wasi/api.h>

#include <trace.h>

uint64_t wasi_monotonic_time_us(void)
{
    __wasi_timestamp_t timestamp;
    __wasi_errno_t err = __wasi_clock_time_get(
        __WASI_CLOCKID_MONOTONIC,
        1000, // precision in nanoseconds
        &timestamp
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        TRACE("wasi_monotonic_time_us: clock_time_get failed with errno %d\n", err);
        return 0;
    }

    // Convert nanoseconds to microseconds
    return timestamp / 1000;
}

uint64_t wasi_monotonic_time_ns(void)
{
    __wasi_timestamp_t timestamp;
    __wasi_errno_t err = __wasi_clock_time_get(
        __WASI_CLOCKID_MONOTONIC,
        1, // precision in nanoseconds
        &timestamp
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        TRACE("wasi_monotonic_time_ns: clock_time_get failed with errno %d\n", err);
        return 0;
    }

    return timestamp;
}

void wasi_time(struct timespec *t)
{
    __wasi_timestamp_t timestamp;
    __wasi_errno_t err = __wasi_clock_time_get(
        __WASI_CLOCKID_REALTIME,
        1, // precision in nanoseconds
        &timestamp
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        fprintf(stderr, "wasi_time: clock_time_get failed with errno %d\n", err);
        t->tv_sec = 0;
        t->tv_nsec = 0;
        return;
    }

    // Convert nanoseconds to seconds and nanoseconds
    t->tv_sec = timestamp / 1000000000ULL;
    t->tv_nsec = timestamp % 1000000000ULL;
}

void wasi_monotonic_time(struct timespec *t)
{
    __wasi_timestamp_t timestamp;
    __wasi_errno_t err = __wasi_clock_time_get(
        __WASI_CLOCKID_MONOTONIC,
        1, // precision in nanoseconds
        &timestamp
    );

    if (err != __WASI_ERRNO_SUCCESS) {
        fprintf(stderr, "wasi_monotonic_time: clock_time_get failed with errno %d\n", err);
        t->tv_sec = 0;
        t->tv_nsec = 0;
        return;
    }

    // Convert nanoseconds to seconds and nanoseconds
    t->tv_sec = timestamp / 1000000000ULL;
    t->tv_nsec = timestamp % 1000000000ULL;
}
