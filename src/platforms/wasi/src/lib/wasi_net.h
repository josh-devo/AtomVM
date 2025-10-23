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

#ifndef _WASI_NET_H_
#define _WASI_NET_H_

#include "context.h"
#include "term.h"

/**
 * @brief Create socket driver data
 * @return Pointer to socket driver data structure
 */
void *wasi_socket_driver_create_data(void);

/**
 * @brief Delete socket driver data
 * @param data Socket driver data to free
 */
void wasi_socket_driver_delete_data(void *data);

/**
 * @brief Initialize socket (connect, bind, listen)
 * @param ctx Port context
 * @param params Initialization parameters
 * @return OK_ATOM or error tuple
 */
term wasi_socket_driver_do_init(Context *ctx, term params);

/**
 * @brief Send data on socket
 * @param ctx Port context
 * @param buffer Data to send
 * @return {ok, BytesSent} or {error, Reason}
 */
term wasi_socket_driver_do_send(Context *ctx, term buffer);

/**
 * @brief Receive data from socket
 * @param ctx Port context
 * @param pid Requesting process pid
 * @param ref Reference for reply
 * @param length Max length to receive
 * @param timeout Timeout in milliseconds
 */
void wasi_socket_driver_do_recv(Context *ctx, term pid, term ref, term length, term timeout);

/**
 * @brief Close socket
 * @param ctx Port context
 */
void wasi_socket_driver_do_close(Context *ctx);

/**
 * @brief Get local port number
 * @param ctx Port context
 * @return {ok, Port} or {error, Reason}
 */
term wasi_socket_driver_get_port(Context *ctx);

/**
 * @brief Get local socket name (address and port)
 * @param ctx Port context
 * @return {ok, {Address, Port}} or {error, Reason}
 */
term wasi_socket_driver_sockname(Context *ctx);

/**
 * @brief Get peer socket name (address and port)
 * @param ctx Port context
 * @return {ok, {Address, Port}} or {error, Reason}
 */
term wasi_socket_driver_peername(Context *ctx);

/**
 * @brief Accept connection on listening socket
 * @param ctx Port context
 * @param pid Requesting process pid
 * @param ref Reference for reply
 * @param timeout Timeout in milliseconds
 */
void wasi_socket_driver_do_accept(Context *ctx, term pid, term ref, term timeout);

#endif // _WASI_NET_H_
