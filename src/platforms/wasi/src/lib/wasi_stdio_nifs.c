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

#include "wasi_stdio_nifs.h"

#include "atom.h"
#include "context.h"
#include "defaultatoms.h"
#include "globalcontext.h"
#include "interop.h"
#include "memory.h"
#include "nifs.h"
#include "platform_defaultatoms.h"
#include "term.h"

#include <wasi/wasip2.h>
#include <string.h>
#include <stdlib.h>

// #define ENABLE_TRACE
#include "trace.h"

#ifdef ENABLE_TRACE
#define TRACE_STDIO TRACE
#else
#define TRACE_STDIO(...)
#endif

// Global stdin/stdout streams (initialized once)
static stdin_own_input_stream_t g_stdin = {0};
static stdout_own_output_stream_t g_stdout = {0};
static bool g_stdio_initialized = false;

static void init_stdio_streams(void)
{
    if (!g_stdio_initialized) {
        g_stdin = stdin_get_stdin();
        g_stdout = stdout_get_stdout();
        g_stdio_initialized = true;
        TRACE_STDIO("wasi_stdio: streams initialized\n");
    }
}

// io:get_chars(Device, Count) -> {ok, Binary} | eof | {error, Reason}
// Device is typically 'standard_io' or an io_device atom
// Count is the number of characters/bytes to read
static term nif_io_get_chars(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    init_stdio_streams();

    // argv[0] is the device (typically standard_io atom)
    // argv[1] is the count

    if (!term_is_integer(argv[1])) {
        TRACE_STDIO("wasi_stdio: get_chars - count is not an integer\n");
        return ERROR_ATOM;
    }

    avm_int64_t count = term_to_int(argv[1]);
    if (count < 0) {
        count = 4096;  // Default chunk size
    } else if (count == 0) {
        count = 4096;  // Read in reasonable chunks
    }

    TRACE_STDIO("wasi_stdio: get_chars - reading %lld bytes\n", count);

    // Read from stdin using blocking read
    wasip2_list_u8_t read_data;
    streams_stream_error_t err;

    bool result = streams_method_input_stream_blocking_read(
        streams_borrow_input_stream(g_stdin),
        (uint64_t)count,
        &read_data,
        &err
    );

    if (!result) {
        // Check if stream is closed (EOF) - STREAMS_STREAM_ERROR_CLOSED = 1
        if (err.tag == 1) {
            TRACE_STDIO("wasi_stdio: get_chars - stream closed (EOF)\n");
            return globalcontext_make_atom(glb, ATOM_STR("\x3", "eof"));
        }

        TRACE_STDIO("wasi_stdio: get_chars - read failed: %d\n", err.tag);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x3", "eio")));
        return error_tuple;
    }

    // Check for EOF (zero-length read)
    if (read_data.len == 0) {
        TRACE_STDIO("wasi_stdio: get_chars - EOF (zero length)\n");
        wasip2_list_u8_free(&read_data);
        return globalcontext_make_atom(glb, ATOM_STR("\x3", "eof"));
    }

    TRACE_STDIO("wasi_stdio: get_chars - read %zu bytes\n", read_data.len);

    // Create binary term from read data
    term bin_term = term_from_literal_binary(
        read_data.ptr,
        read_data.len,
        &ctx->heap,
        glb
    );

    // Free the read buffer using WASI cleanup function
    wasip2_list_u8_free(&read_data);

    // Return {ok, Binary}
    term ok_tuple = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(ok_tuple, 0, OK_ATOM);
    term_put_tuple_element(ok_tuple, 1, bin_term);

    return ok_tuple;
}

// io:put_chars(Device, Data) -> ok | {error, Reason}
// Device is typically 'standard_io' or an io_device atom
// Data is a binary or iolist to write
static term nif_io_put_chars(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    init_stdio_streams();

    // argv[0] is the device (typically standard_io atom)
    // argv[1] is the data to write

    term data_term = argv[1];

    // For now, only support binary data
    if (!term_is_binary(data_term)) {
        TRACE_STDIO("wasi_stdio: put_chars - data is not binary\n");
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x6", "badarg")));
        return error_tuple;
    }

    const uint8_t *data = (const uint8_t *) term_binary_data(data_term);
    size_t data_len = term_binary_size(data_term);

    TRACE_STDIO("wasi_stdio: put_chars - writing %zu bytes\n", data_len);

    // Write to stdout (no explicit flush - stdout is line-buffered by default)
    wasip2_list_u8_t write_data = {
        .ptr = (uint8_t *)data,
        .len = data_len
    };

    streams_stream_error_t err;

    bool result = streams_method_output_stream_write(
        streams_borrow_output_stream(g_stdout),
        &write_data,
        &err
    );

    if (!result) {
        TRACE_STDIO("wasi_stdio: put_chars - write failed: %d\n", err);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x3", "eio")));
        return error_tuple;
    }

    TRACE_STDIO("wasi_stdio: put_chars - write successful\n");
    return OK_ATOM;
}

// NIF table
static const struct Nif io_get_chars_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_io_get_chars
};

static const struct Nif io_put_chars_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_io_put_chars
};

const struct Nif *wasi_stdio_nifs_get_nif(const char *nifname)
{
    if (strcmp("io:get_chars/2", nifname) == 0) {
        return &io_get_chars_nif;
    }
    if (strcmp("io:put_chars/2", nifname) == 0) {
        return &io_put_chars_nif;
    }
    return NULL;
}
