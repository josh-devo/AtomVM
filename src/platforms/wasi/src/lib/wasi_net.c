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

#include "wasi_net.h"

#include "atom.h"
#include "context.h"
#include "defaultatoms.h"
#include "globalcontext.h"
#include "interop.h"
#include "mailbox.h"
#include "port.h"
#include "term.h"
#include "utils.h"
#include "platform_defaultatoms.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Enable WASI Preview 2 APIs
#define __wasilibc_use_wasip2
#include <wasi/wasip2.h>

#define ENABLE_TRACE
#include "trace.h"

#define BUFSIZE 512

/**
 * @brief Socket driver data for WASI
 */
typedef struct WasiSocketDriverData
{
    // WASI socket handles
    tcp_own_tcp_socket_t wasi_socket;
    streams_own_input_stream_t input_stream;
    streams_own_output_stream_t output_stream;
    network_own_network_t network;

    // Connection state
    bool connected;
    bool listening;
    bool has_socket;
    bool has_streams;
    bool has_network;

    // AtomVM socket driver fields
    term proto;                    // TCP_ATOM or UDP_ATOM
    uint16_t port;
    term controlling_process;
    bool binary;
    bool active;
    size_t buffer;
} WasiSocketDriverData;

// Atom definitions (TODO: move to platform_defaultatoms if needed)
static const char *gen_tcp_moniker_atom = ATOM_STR("\xC", "$avm_gen_tcp");
static const char *native_tcp_module_atom = ATOM_STR("\xC", "gen_tcp_inet");

//
// Helper functions
//

static inline term create_tcp_socket_wrapper(term pid, Heap *heap, GlobalContext *global)
{
    term tuple = term_alloc_tuple(3, heap);
    term_put_tuple_element(tuple, 0, globalcontext_make_atom(global, gen_tcp_moniker_atom));
    term_put_tuple_element(tuple, 1, pid);
    term_put_tuple_element(tuple, 2, globalcontext_make_atom(global, native_tcp_module_atom));
    return tuple;
}

static term wasi_error_to_atom(tcp_error_code_t err, GlobalContext *glb)
{
    switch (err) {
        case NETWORK_ERROR_CODE_ACCESS_DENIED:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "eacces"));
        case NETWORK_ERROR_CODE_CONNECTION_REFUSED:
            return globalcontext_make_atom(glb, ATOM_STR("\xB", "econnrefused"));
        case NETWORK_ERROR_CODE_CONNECTION_RESET:
            return globalcontext_make_atom(glb, ATOM_STR("\xA", "econnreset"));
        case NETWORK_ERROR_CODE_CONNECTION_ABORTED:
            return globalcontext_make_atom(glb, ATOM_STR("\xD", "econnaborted"));
        case NETWORK_ERROR_CODE_TIMEOUT:
            return globalcontext_make_atom(glb, ATOM_STR("\xA", "etimedout"));
        case NETWORK_ERROR_CODE_ADDRESS_IN_USE:
            return globalcontext_make_atom(glb, ATOM_STR("\xB", "eaddrinuse"));
        case NETWORK_ERROR_CODE_ADDRESS_NOT_BINDABLE:
            return globalcontext_make_atom(glb, ATOM_STR("\xD", "eaddrnotavail"));
        case NETWORK_ERROR_CODE_WOULD_BLOCK:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "eagain"));
        case NETWORK_ERROR_CODE_NOT_SUPPORTED:
            return globalcontext_make_atom(glb, ATOM_STR("\xB", "enotsupported"));
        case NETWORK_ERROR_CODE_INVALID_ARGUMENT:
            return BADARG_ATOM;
        case NETWORK_ERROR_CODE_OUT_OF_MEMORY:
            return OUT_OF_MEMORY_ATOM;
        default:
            return globalcontext_make_atom(glb, ATOM_STR("\x7", "unknown"));
    }
}

// Forward declarations
static NativeHandlerResult wasi_socket_consume_mailbox(Context *ctx);
static term init_client_tcp_socket(Context *ctx, WasiSocketDriverData *socket_data, term params);
static term init_server_tcp_socket(Context *ctx, WasiSocketDriverData *socket_data, term params);

//
// Public API
//

void *wasi_socket_driver_create_data(void)
{
    struct WasiSocketDriverData *data = calloc(1, sizeof(struct WasiSocketDriverData));
    if (data == NULL) {
        return NULL;
    }

    // Initialize socket state
    data->has_socket = false;
    data->has_streams = false;
    data->has_network = false;
    data->connected = false;
    data->listening = false;

    // Initialize AtomVM socket fields
    data->proto = term_invalid_term();
    data->port = 0;
    data->controlling_process = term_invalid_term();
    data->binary = false;
    data->active = true;
    data->buffer = BUFSIZE;

    return (void *) data;
}

void wasi_socket_driver_delete_data(void *data)
{
    if (data == NULL) {
        return;
    }

    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) data;

    // Drop WASI resources
    if (socket_data->has_streams) {
        streams_input_stream_drop_own(socket_data->input_stream);
        streams_output_stream_drop_own(socket_data->output_stream);
    }
    if (socket_data->has_socket) {
        tcp_tcp_socket_drop_own(socket_data->wasi_socket);
    }
    if (socket_data->has_network) {
        network_network_drop_own(socket_data->network);
    }

    free(data);
}

static term do_connect(WasiSocketDriverData *socket_data, Context *ctx, term address, term port)
{
    GlobalContext *glb = ctx->global;

    // Convert address string to IP address
    if (!term_is_list(address)) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }

    int ok;
    char *addr_str = interop_term_to_string(address, &ok);
    if (!ok) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }

    // Parse IP address (simple IPv4 parser for now)
    unsigned int a, b, c, d;
    int parsed = sscanf(addr_str, "%u.%u.%u.%u", &a, &b, &c, &d);
    free(addr_str);

    if (parsed != 4 || a > 255 || b > 255 || c > 255 || d > 255) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }

    // Build WASI socket address
    tcp_ip_socket_address_t remote_addr = {
        .tag = NETWORK_IP_SOCKET_ADDRESS_IPV4,
        .val.ipv4 = {
            .address = {{(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d}},
            .port = (uint16_t)term_to_int(port)
        }
    };

    // Start connect
    tcp_error_code_t err;
    bool result = tcp_method_tcp_socket_start_connect(
        tcp_borrow_tcp_socket(socket_data->wasi_socket),
        network_borrow_network(socket_data->network),
        &remote_addr,
        &err
    );

    if (!result) {
        TRACE("wasi_net: start_connect failed: %d\n", err);
        return port_create_error_tuple(ctx, wasi_error_to_atom(err, glb));
    }

    // Finish connect - poll until connected or error
    // WASI Preview 2 sockets are async, so we need to retry on WOULD_BLOCK
    tcp_tuple2_own_input_stream_own_output_stream_t streams;
    int retry_count = 0;
    const int MAX_RETRIES = 1000;  // Prevent infinite loop

    while (retry_count < MAX_RETRIES) {
        result = tcp_method_tcp_socket_finish_connect(
            tcp_borrow_tcp_socket(socket_data->wasi_socket),
            &streams,
            &err
        );

        if (result) {
            // Success!
            break;
        }

        // Check if it's a retryable error
        if (err == NETWORK_ERROR_CODE_WOULD_BLOCK) {
            // Connection in progress, retry
            TRACE("wasi_net: finish_connect would block, retrying (%d)...\n", retry_count);
            retry_count++;
            // Small yield to avoid busy-waiting
            // Note: In a full implementation, we'd use proper async polling here
            continue;
        } else {
            // Non-retryable error
            TRACE("wasi_net: finish_connect failed: %d\n", err);
            return port_create_error_tuple(ctx, wasi_error_to_atom(err, glb));
        }
    }

    if (retry_count >= MAX_RETRIES) {
        TRACE("wasi_net: finish_connect timed out after %d retries\n", retry_count);
        return port_create_error_tuple(ctx, globalcontext_make_atom(glb, ATOM_STR("\xA", "etimedout")));
    }

    // Store streams
    socket_data->input_stream = streams.f0;
    socket_data->output_stream = streams.f1;
    socket_data->has_streams = true;
    socket_data->connected = true;

    TRACE("wasi_net: connected to %u.%u.%u.%u:%u\n", a, b, c, d, (unsigned)term_to_int(port));

    return OK_ATOM;
}

static term init_client_tcp_socket(Context *ctx, WasiSocketDriverData *socket_data, term params)
{
    GlobalContext *glb = ctx->global;

    // Create TCP socket
    tcp_create_socket_ip_address_family_t family = NETWORK_IP_ADDRESS_FAMILY_IPV4;
    tcp_create_socket_own_tcp_socket_t socket;
    tcp_create_socket_error_code_t err;

    bool result = tcp_create_socket_create_tcp_socket(family, &socket, &err);
    if (!result) {
        TRACE("wasi_net: create_tcp_socket failed: %d\n", err);
        return port_create_error_tuple(ctx, wasi_error_to_atom(err, glb));
    }

    socket_data->wasi_socket = socket;
    socket_data->has_socket = true;

    // Get network capability
    socket_data->network = instance_network_instance_network();
    socket_data->has_network = true;

    // Connect to remote address
    term address = interop_proplist_get_value(params, ADDRESS_ATOM);
    term port = interop_proplist_get_value(params, PORT_ATOM);

    term ret = do_connect(socket_data, ctx, address, port);
    if (ret != OK_ATOM) {
        // Clean up on error
        if (socket_data->has_socket) {
            tcp_tcp_socket_drop_own(socket_data->wasi_socket);
            socket_data->has_socket = false;
        }
        if (socket_data->has_network) {
            network_network_drop_own(socket_data->network);
            socket_data->has_network = false;
        }
    }

    return ret;
}

static term init_server_tcp_socket(Context *ctx, WasiSocketDriverData *socket_data, term params)
{
    GlobalContext *glb = ctx->global;

    // Create TCP socket
    tcp_create_socket_ip_address_family_t family = NETWORK_IP_ADDRESS_FAMILY_IPV4;
    tcp_create_socket_own_tcp_socket_t socket;
    tcp_create_socket_error_code_t err;

    bool result = tcp_create_socket_create_tcp_socket(family, &socket, &err);
    if (!result) {
        TRACE("wasi_net: create_tcp_socket failed: %d\n", err);
        return port_create_error_tuple(ctx, wasi_error_to_atom(err, glb));
    }

    socket_data->wasi_socket = socket;
    socket_data->has_socket = true;

    // Get network capability
    socket_data->network = instance_network_instance_network();
    socket_data->has_network = true;

    TRACE("wasi_net: server socket created (port %d)\n", socket_data->port);

    // Now perform bind and listen
    // Bind to the specified port
    tcp_ip_socket_address_t local_addr = {
        .tag = NETWORK_IP_SOCKET_ADDRESS_IPV4,
        .val.ipv4 = {
            .address = {{0, 0, 0, 0}},  // INADDR_ANY
            .port = socket_data->port
        }
    };

    // Start bind
    tcp_error_code_t tcp_err;
    result = tcp_method_tcp_socket_start_bind(
        tcp_borrow_tcp_socket(socket_data->wasi_socket),
        network_borrow_network(socket_data->network),
        &local_addr,
        &tcp_err
    );

    if (!result) {
        TRACE("wasi_net: start_bind failed: %d\n", tcp_err);
        return port_create_error_tuple(ctx, wasi_error_to_atom(tcp_err, glb));
    }

    // Finish bind - poll until ready
    int retry_count = 0;
    const int MAX_RETRIES = 1000;

    while (retry_count < MAX_RETRIES) {
        result = tcp_method_tcp_socket_finish_bind(
            tcp_borrow_tcp_socket(socket_data->wasi_socket),
            &tcp_err
        );

        if (result) {
            break;
        }

        if (tcp_err == NETWORK_ERROR_CODE_WOULD_BLOCK) {
            TRACE("wasi_net: finish_bind would block, retrying (%d)...\n", retry_count);
            retry_count++;
            continue;
        } else {
            TRACE("wasi_net: finish_bind failed: %d\n", tcp_err);
            return port_create_error_tuple(ctx, wasi_error_to_atom(tcp_err, glb));
        }
    }

    if (retry_count >= MAX_RETRIES) {
        TRACE("wasi_net: finish_bind timed out after %d retries\n", retry_count);
        return port_create_error_tuple(ctx, globalcontext_make_atom(glb, ATOM_STR("\xA", "etimedout")));
    }

    TRACE("wasi_net: bound to port %d\n", socket_data->port);

    // Start listen
    result = tcp_method_tcp_socket_start_listen(
        tcp_borrow_tcp_socket(socket_data->wasi_socket),
        &tcp_err
    );

    if (!result) {
        TRACE("wasi_net: start_listen failed: %d\n", tcp_err);
        return port_create_error_tuple(ctx, wasi_error_to_atom(tcp_err, glb));
    }

    // Finish listen - poll until ready
    retry_count = 0;

    while (retry_count < MAX_RETRIES) {
        result = tcp_method_tcp_socket_finish_listen(
            tcp_borrow_tcp_socket(socket_data->wasi_socket),
            &tcp_err
        );

        if (result) {
            break;
        }

        if (tcp_err == NETWORK_ERROR_CODE_WOULD_BLOCK) {
            TRACE("wasi_net: finish_listen would block, retrying (%d)...\n", retry_count);
            retry_count++;
            continue;
        } else {
            TRACE("wasi_net: finish_listen failed: %d\n", tcp_err);
            return port_create_error_tuple(ctx, wasi_error_to_atom(tcp_err, glb));
        }
    }

    if (retry_count >= MAX_RETRIES) {
        TRACE("wasi_net: finish_listen timed out after %d retries\n", retry_count);
        return port_create_error_tuple(ctx, globalcontext_make_atom(glb, ATOM_STR("\xA", "etimedout")));
    }

    socket_data->listening = true;
    TRACE("wasi_net: listening on port %d\n", socket_data->port);

    return OK_ATOM;
}

term wasi_socket_driver_do_init(Context *ctx, term params)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;

    if (!term_is_list(params)) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }

    // Get protocol
    term proto = interop_proplist_get_value(params, PROTO_ATOM);
    if (term_is_nil(proto)) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }
    socket_data->proto = proto;

    // Get controlling process
    term controlling_process = interop_proplist_get_value_default(params, CONTROLLING_PROCESS_ATOM, term_invalid_term());
    if (!(term_is_invalid_term(controlling_process) || term_is_pid(controlling_process))) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }
    socket_data->controlling_process = controlling_process;

    // Get binary flag
    term binary = interop_proplist_get_value_default(params, BINARY_ATOM, FALSE_ATOM);
    if (!(binary == TRUE_ATOM || binary == FALSE_ATOM)) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }
    socket_data->binary = binary == TRUE_ATOM;

    // Get buffer size
    term buffer = interop_proplist_get_value_default(params, BUFFER_ATOM, term_from_int(BUFSIZE));
    if (!term_is_integer(buffer)) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }
    socket_data->buffer = (size_t) term_to_int(buffer);

    // Get active flag
    term active = interop_proplist_get_value_default(params, ACTIVE_ATOM, FALSE_ATOM);
    if (!(active == TRUE_ATOM || active == FALSE_ATOM)) {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }
    socket_data->active = active == TRUE_ATOM;

    // Get port number (required for both client and server)
    term port_term = interop_proplist_get_value(params, PORT_ATOM);
    if (!term_is_invalid_term(port_term) && term_is_integer(port_term)) {
        socket_data->port = (uint16_t)term_to_int(port_term);
    }

    // Initialize based on protocol and mode
    if (proto == TCP_ATOM) {
        term connect = interop_proplist_get_value_default(params, CONNECT_ATOM, FALSE_ATOM);
        term listen = interop_proplist_get_value_default(params, globalcontext_make_atom(ctx->global, ATOM_STR("\x6", "listen")), FALSE_ATOM);

        if (connect == TRUE_ATOM) {
            // Client mode
            return init_client_tcp_socket(ctx, socket_data, params);
        } else if (listen == TRUE_ATOM) {
            // Server mode - initialize socket, then call listen separately
            return init_server_tcp_socket(ctx, socket_data, params);
        } else {
            return port_create_error_tuple(ctx, BADARG_ATOM);
        }
    } else if (proto == UDP_ATOM) {
        // UDP - TODO: implement later
        return port_create_error_tuple(ctx, globalcontext_make_atom(ctx->global, ATOM_STR("\xD", "not_supported")));
    } else {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }
}

term wasi_socket_driver_do_send(Context *ctx, term buffer)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;

    if (!socket_data->connected || !socket_data->has_streams) {
        return port_create_error_tuple(ctx, CLOSED_ATOM);
    }

    // Get binary data to send
    const char *data;
    size_t len;

    if (term_is_binary(buffer)) {
        data = (const char *) term_binary_data(buffer);
        len = term_binary_size(buffer);
    } else if (term_is_list(buffer)) {
        // Convert list to binary
        int ok;
        char *str = interop_list_to_string(buffer, &ok);
        if (!ok) {
            return port_create_error_tuple(ctx, BADARG_ATOM);
        }
        data = str;
        len = strlen(str);
        // TODO: need to free str after write
    } else {
        return port_create_error_tuple(ctx, BADARG_ATOM);
    }

    // Write to output stream
    wasip2_list_u8_t wasi_buffer = {
        .ptr = (uint8_t*)data,
        .len = len
    };

    streams_stream_error_t err;

    bool result = streams_method_output_stream_write(
        streams_borrow_output_stream(socket_data->output_stream),
        &wasi_buffer,
        &err
    );

    if (!result) {
        TRACE("wasi_net: output_stream_write failed: %d\n", err);
        return port_create_error_tuple(ctx, CLOSED_ATOM);
    }

    // Flush to ensure data is sent
    bool flush_result = streams_method_output_stream_blocking_flush(
        streams_borrow_output_stream(socket_data->output_stream),
        &err
    );

    if (!flush_result) {
        TRACE("wasi_net: output_stream_flush failed: %d\n", err);
        return port_create_error_tuple(ctx, CLOSED_ATOM);
    }

    // Assume all bytes written on success
    uint64_t bytes_written = len;
    TRACE("wasi_net: sent %llu bytes (flushed)\n", (unsigned long long)bytes_written);

    // Return {ok, BytesSent}
    term result_tuple = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(result_tuple, 0, OK_ATOM);
    term_put_tuple_element(result_tuple, 1, term_from_int(bytes_written));

    return result_tuple;
}

term wasi_socket_driver_do_listen(Context *ctx, term backlog)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;
    GlobalContext *glb = ctx->global;

    if (!socket_data->has_socket || !socket_data->has_network) {
        return port_create_error_tuple(ctx, CLOSED_ATOM);
    }

    UNUSED(backlog);  // WASI doesn't expose backlog parameter

    // Bind to the specified port (stored during init)
    tcp_ip_socket_address_t local_addr = {
        .tag = NETWORK_IP_SOCKET_ADDRESS_IPV4,
        .val.ipv4 = {
            .address = {{0, 0, 0, 0}},  // INADDR_ANY
            .port = socket_data->port
        }
    };

    // Start bind
    tcp_error_code_t err;
    bool result = tcp_method_tcp_socket_start_bind(
        tcp_borrow_tcp_socket(socket_data->wasi_socket),
        network_borrow_network(socket_data->network),
        &local_addr,
        &err
    );

    if (!result) {
        TRACE("wasi_net: start_bind failed: %d\n", err);
        return port_create_error_tuple(ctx, wasi_error_to_atom(err, glb));
    }

    // Finish bind - poll until ready
    int retry_count = 0;
    const int MAX_RETRIES = 1000;

    while (retry_count < MAX_RETRIES) {
        result = tcp_method_tcp_socket_finish_bind(
            tcp_borrow_tcp_socket(socket_data->wasi_socket),
            &err
        );

        if (result) {
            break;
        }

        if (err == NETWORK_ERROR_CODE_WOULD_BLOCK) {
            TRACE("wasi_net: finish_bind would block, retrying (%d)...\n", retry_count);
            retry_count++;
            continue;
        } else {
            TRACE("wasi_net: finish_bind failed: %d\n", err);
            return port_create_error_tuple(ctx, wasi_error_to_atom(err, glb));
        }
    }

    if (retry_count >= MAX_RETRIES) {
        TRACE("wasi_net: finish_bind timed out after %d retries\n", retry_count);
        return port_create_error_tuple(ctx, globalcontext_make_atom(glb, ATOM_STR("\xA", "etimedout")));
    }

    TRACE("wasi_net: bound to port %d\n", socket_data->port);

    // Start listen
    result = tcp_method_tcp_socket_start_listen(
        tcp_borrow_tcp_socket(socket_data->wasi_socket),
        &err
    );

    if (!result) {
        TRACE("wasi_net: start_listen failed: %d\n", err);
        return port_create_error_tuple(ctx, wasi_error_to_atom(err, glb));
    }

    // Finish listen - poll until ready
    retry_count = 0;

    while (retry_count < MAX_RETRIES) {
        result = tcp_method_tcp_socket_finish_listen(
            tcp_borrow_tcp_socket(socket_data->wasi_socket),
            &err
        );

        if (result) {
            break;
        }

        if (err == NETWORK_ERROR_CODE_WOULD_BLOCK) {
            TRACE("wasi_net: finish_listen would block, retrying (%d)...\n", retry_count);
            retry_count++;
            continue;
        } else {
            TRACE("wasi_net: finish_listen failed: %d\n", err);
            return port_create_error_tuple(ctx, wasi_error_to_atom(err, glb));
        }
    }

    if (retry_count >= MAX_RETRIES) {
        TRACE("wasi_net: finish_listen timed out after %d retries\n", retry_count);
        return port_create_error_tuple(ctx, globalcontext_make_atom(glb, ATOM_STR("\xA", "etimedout")));
    }

    socket_data->listening = true;
    TRACE("wasi_net: listening on port %d\n", socket_data->port);

    return OK_ATOM;
}

void wasi_socket_driver_do_recv(Context *ctx, term pid, term ref, term length, term timeout)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;
    GlobalContext *glb = ctx->global;

    if (!socket_data->connected || !socket_data->has_streams) {
        // Send {Ref, {error, closed}}
        BEGIN_WITH_STACK_HEAP(TUPLE_SIZE(2) + TUPLE_SIZE(2), heap);
        term error_tuple = term_alloc_tuple(2, &heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, CLOSED_ATOM);

        term reply_tuple = term_alloc_tuple(2, &heap);
        term_put_tuple_element(reply_tuple, 0, ref);
        term_put_tuple_element(reply_tuple, 1, error_tuple);

        port_send_message(ctx->global, pid, reply_tuple);
        END_WITH_STACK_HEAP(heap, ctx->global);
        return;
    }

    // Read from input stream using blocking_read to wait for data
    uint64_t max_len = term_to_int(length);
    if (max_len == 0) {
        // Length 0 means read all available, use a reasonable buffer size
        max_len = 8192;
    }

    wasip2_list_u8_t buffer;
    streams_stream_error_t err;

    bool result = streams_method_input_stream_blocking_read(
        streams_borrow_input_stream(socket_data->input_stream),
        max_len,
        &buffer,
        &err
    );

    if (!result) {
        TRACE("wasi_net: input_stream_blocking_read failed: %d\n", err);
        // Send {Ref, {error, closed}}
        BEGIN_WITH_STACK_HEAP(TUPLE_SIZE(2) + TUPLE_SIZE(2), heap);
        term error_tuple = term_alloc_tuple(2, &heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, CLOSED_ATOM);

        term reply_tuple = term_alloc_tuple(2, &heap);
        term_put_tuple_element(reply_tuple, 0, ref);
        term_put_tuple_element(reply_tuple, 1, error_tuple);

        port_send_message(ctx->global, pid, reply_tuple);
        END_WITH_STACK_HEAP(heap, ctx->global);
        return;
    }

    TRACE("wasi_net: received %lu bytes\n", (unsigned long)buffer.len);

    // Create packet term
    term packet;
    if (socket_data->binary) {
        packet = term_from_literal_binary((void *)buffer.ptr, buffer.len, &ctx->heap, glb);
    } else {
        packet = term_from_string((const uint8_t *)buffer.ptr, buffer.len, &ctx->heap);
    }

    // Free WASI buffer
    wasip2_list_u8_free(&buffer);

    // Send {Ref, {ok, Packet}}
    BEGIN_WITH_STACK_HEAP(TUPLE_SIZE(2) + TUPLE_SIZE(2), heap);
    term ok_tuple = term_alloc_tuple(2, &heap);
    term_put_tuple_element(ok_tuple, 0, OK_ATOM);
    term_put_tuple_element(ok_tuple, 1, packet);

    term reply_tuple = term_alloc_tuple(2, &heap);
    term_put_tuple_element(reply_tuple, 0, ref);
    term_put_tuple_element(reply_tuple, 1, ok_tuple);

    port_send_message(ctx->global, pid, reply_tuple);
    END_WITH_STACK_HEAP(heap, ctx->global);
}

void wasi_socket_driver_do_close(Context *ctx)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;

    if (socket_data->has_streams) {
        streams_input_stream_drop_own(socket_data->input_stream);
        streams_output_stream_drop_own(socket_data->output_stream);
        socket_data->has_streams = false;
    }

    if (socket_data->has_socket) {
        tcp_tcp_socket_drop_own(socket_data->wasi_socket);
        socket_data->has_socket = false;
    }

    if (socket_data->has_network) {
        network_network_drop_own(socket_data->network);
        socket_data->has_network = false;
    }

    socket_data->connected = false;
    socket_data->listening = false;

    TRACE("wasi_net: socket closed\n");
}

term wasi_socket_driver_get_port(Context *ctx)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;

    // TODO: implement - need to get local port from socket
    UNUSED(socket_data);

    return port_create_error_tuple(ctx, globalcontext_make_atom(ctx->global, ATOM_STR("\xD", "not_supported")));
}

term wasi_socket_driver_sockname(Context *ctx)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;

    // TODO: implement - need wasi_sockets API for getsockname
    UNUSED(socket_data);

    return port_create_error_tuple(ctx, globalcontext_make_atom(ctx->global, ATOM_STR("\xD", "not_supported")));
}

term wasi_socket_driver_peername(Context *ctx)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;

    // TODO: implement - need wasi_sockets API for getpeername
    UNUSED(socket_data);

    return port_create_error_tuple(ctx, globalcontext_make_atom(ctx->global, ATOM_STR("\xD", "not_supported")));
}

void wasi_socket_driver_do_accept(Context *ctx, term pid, term ref, term timeout)
{
    WasiSocketDriverData *socket_data = (WasiSocketDriverData *) ctx->platform_data;
    GlobalContext *glb = ctx->global;

    UNUSED(timeout);  // TODO: implement timeout support

    if (!socket_data->listening) {
        // Send {Ref, {error, einval}}
        BEGIN_WITH_STACK_HEAP(TUPLE_SIZE(2) + TUPLE_SIZE(2), heap);
        term error_tuple = term_alloc_tuple(2, &heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, globalcontext_make_atom(glb, ATOM_STR("\x6", "einval")));

        term reply_tuple = term_alloc_tuple(2, &heap);
        term_put_tuple_element(reply_tuple, 0, ref);
        term_put_tuple_element(reply_tuple, 1, error_tuple);

        port_send_message(ctx->global, pid, reply_tuple);
        END_WITH_STACK_HEAP(heap, ctx->global);
        return;
    }

    // Get pollable for the socket to wait for accept readiness
    tcp_own_pollable_t pollable = tcp_method_tcp_socket_subscribe(
        tcp_borrow_tcp_socket(socket_data->wasi_socket)
    );

    // Accept connection - block until a client connects
    tcp_tuple3_own_tcp_socket_own_input_stream_own_output_stream_t accept_result;
    tcp_error_code_t err;

    while (true) {
        bool result = tcp_method_tcp_socket_accept(
            tcp_borrow_tcp_socket(socket_data->wasi_socket),
            &accept_result,
            &err
        );

        if (result) {
            // Success! We have a new client connection
            poll_pollable_drop_own(pollable);
            break;
        }

        if (err == NETWORK_ERROR_CODE_WOULD_BLOCK) {
            TRACE("wasi_net: accept would block, waiting for connection...\n");
            // Block until the socket is ready for accept
            poll_method_pollable_block(poll_borrow_pollable(pollable));
            // Try accept again after unblocking
            continue;
        } else {
            TRACE("wasi_net: accept failed: %d\n", err);
            poll_pollable_drop_own(pollable);
            // Send {Ref, {error, Reason}}
            BEGIN_WITH_STACK_HEAP(TUPLE_SIZE(2) + TUPLE_SIZE(2), heap);
            term error_tuple = term_alloc_tuple(2, &heap);
            term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
            term_put_tuple_element(error_tuple, 1, wasi_error_to_atom(err, glb));

            term reply_tuple = term_alloc_tuple(2, &heap);
            term_put_tuple_element(reply_tuple, 0, ref);
            term_put_tuple_element(reply_tuple, 1, error_tuple);

            port_send_message(ctx->global, pid, reply_tuple);
            END_WITH_STACK_HEAP(heap, ctx->global);
            return;
        }
    }

    TRACE("wasi_net: accepted client connection\n");

    // Create a new socket context for the client
    Context *new_ctx = context_new(glb);
    new_ctx->native_handler = wasi_socket_consume_mailbox;

    WasiSocketDriverData *client_data = (WasiSocketDriverData *) wasi_socket_driver_create_data();
    if (client_data == NULL) {
        // Send {Ref, {error, enomem}}
        tcp_tcp_socket_drop_own(accept_result.f0);
        streams_input_stream_drop_own(accept_result.f1);
        streams_output_stream_drop_own(accept_result.f2);

        BEGIN_WITH_STACK_HEAP(TUPLE_SIZE(2) + TUPLE_SIZE(2), heap);
        term error_tuple = term_alloc_tuple(2, &heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, globalcontext_make_atom(glb, ATOM_STR("\x6", "enomem")));

        term reply_tuple = term_alloc_tuple(2, &heap);
        term_put_tuple_element(reply_tuple, 0, ref);
        term_put_tuple_element(reply_tuple, 1, error_tuple);

        port_send_message(ctx->global, pid, reply_tuple);
        END_WITH_STACK_HEAP(heap, ctx->global);
        return;
    }

    // Initialize client socket data with values from accepted connection
    client_data->wasi_socket = accept_result.f0;
    client_data->input_stream = accept_result.f1;
    client_data->output_stream = accept_result.f2;
    client_data->has_socket = true;
    client_data->has_streams = true;
    client_data->has_network = false;  // Don't need network for accepted socket
    client_data->connected = true;
    client_data->listening = false;
    client_data->binary = socket_data->binary;  // Inherit from listening socket
    client_data->active = socket_data->active;
    client_data->buffer = socket_data->buffer;
    client_data->controlling_process = pid;
    client_data->port = 0;
    client_data->proto = TCP_ATOM;

    new_ctx->platform_data = client_data;

    // Send {Ref, {ok, ClientSocket}}
    BEGIN_WITH_STACK_HEAP(TUPLE_SIZE(2) + TUPLE_SIZE(2) + TUPLE_SIZE(3), heap);

    // Create socket wrapper {$avm_gen_tcp, Pid, gen_tcp_inet}
    term socket_wrapper = term_alloc_tuple(3, &heap);
    term_put_tuple_element(socket_wrapper, 0, globalcontext_make_atom(glb, gen_tcp_moniker_atom));
    term_put_tuple_element(socket_wrapper, 1, term_from_local_process_id(new_ctx->process_id));
    term_put_tuple_element(socket_wrapper, 2, globalcontext_make_atom(glb, native_tcp_module_atom));

    term ok_tuple = term_alloc_tuple(2, &heap);
    term_put_tuple_element(ok_tuple, 0, OK_ATOM);
    term_put_tuple_element(ok_tuple, 1, socket_wrapper);

    term reply_tuple = term_alloc_tuple(2, &heap);
    term_put_tuple_element(reply_tuple, 0, ref);
    term_put_tuple_element(reply_tuple, 1, ok_tuple);

    TRACE("wasi_net: sending accept reply to pid\n");
    port_send_message(ctx->global, pid, reply_tuple);
    END_WITH_STACK_HEAP(heap, ctx->global);

    TRACE("wasi_net: accept complete\n");
}

//
// Port mailbox handler
//

static NativeHandlerResult wasi_socket_consume_mailbox(Context *ctx)
{
    GlobalContext *glb = ctx->global;

    Message *message = mailbox_first(&ctx->mailbox);
    if (message == NULL) {
        return NativeContinue;
    }

    term msg = message->message;

    TRACE("wasi_net: consume_mailbox received message\n");

    // Parse GenMessage format (used by port:call)
    GenMessage gen_message;
    if (UNLIKELY((port_parse_gen_message(msg, &gen_message) != GenCallMessage)
            || !term_is_tuple(gen_message.req) || term_get_tuple_arity(gen_message.req) < 1)) {
        fprintf(stderr, "Received invalid wasi socket message.\n");
        mailbox_remove_message(&ctx->mailbox, &ctx->heap);
        return NativeContinue;
    }

    term pid = gen_message.pid;
    term ref = gen_message.ref;
    term cmd = gen_message.req;

    term cmd_name = term_get_tuple_element(cmd, 0);

    // {init, Params}
    if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x4", "init"))) {
        term params = term_get_tuple_element(cmd, 1);
        term reply = wasi_socket_driver_do_init(ctx, params);
        port_send_reply(ctx, pid, ref, reply);
    }
    // {send, Buffer}
    else if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x4", "send"))) {
        term buffer = term_get_tuple_element(cmd, 1);
        term reply = wasi_socket_driver_do_send(ctx, buffer);
        port_send_reply(ctx, pid, ref, reply);
    }
    // {recv, Length, Timeout}
    else if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x4", "recv"))) {
        term length = term_get_tuple_element(cmd, 1);
        term timeout = term_get_tuple_element(cmd, 2);
        // recv is async - it will send reply via mailbox
        wasi_socket_driver_do_recv(ctx, pid, ref, length, timeout);
        mailbox_remove_message(&ctx->mailbox, &ctx->heap);
        return NativeContinue;
    }
    // {listen, Backlog}
    else if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x6", "listen"))) {
        term backlog = term_get_tuple_element(cmd, 1);
        term reply = wasi_socket_driver_do_listen(ctx, backlog);
        port_send_reply(ctx, pid, ref, reply);
    }
    // {accept, Timeout}
    else if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x6", "accept"))) {
        term timeout = term_get_tuple_element(cmd, 1);
        // accept is async - it will send reply via mailbox
        wasi_socket_driver_do_accept(ctx, pid, ref, timeout);
        mailbox_remove_message(&ctx->mailbox, &ctx->heap);
        return NativeContinue;
    }
    // {close}
    else if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x5", "close"))) {
        wasi_socket_driver_do_close(ctx);
        port_send_reply(ctx, pid, ref, OK_ATOM);
        mailbox_remove_message(&ctx->mailbox, &ctx->heap);
        return NativeTerminate;
    }
    // {get_port}
    else if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x8", "get_port"))) {
        term reply = wasi_socket_driver_get_port(ctx);
        port_send_reply(ctx, pid, ref, reply);
    }
    // {sockname}
    else if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x8", "sockname"))) {
        term reply = wasi_socket_driver_sockname(ctx);
        port_send_reply(ctx, pid, ref, reply);
    }
    // {peername}
    else if (cmd_name == globalcontext_make_atom(glb, ATOM_STR("\x8", "peername"))) {
        term reply = wasi_socket_driver_peername(ctx);
        port_send_reply(ctx, pid, ref, reply);
    }
    else {
        TRACE("wasi_net: unknown command\n");
        port_send_reply(ctx, pid, ref, port_create_error_tuple(ctx, BADARG_ATOM));
    }

    mailbox_remove_message(&ctx->mailbox, &ctx->heap);

    TRACE("wasi_net: consume_mailbox done\n");
    return NativeContinue;
}

//
// Port initialization
//

Context *wasi_socket_init(GlobalContext *glb, term opts)
{
    UNUSED(opts);

    TRACE("wasi_net: socket_init called\n");

    Context *ctx = context_new(glb);
    void *data = wasi_socket_driver_create_data();
    ctx->native_handler = wasi_socket_consume_mailbox;
    ctx->platform_data = data;

    return ctx;
}
