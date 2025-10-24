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

#ifndef _WASI_FILE_NIFS_H_
#define _WASI_FILE_NIFS_H_

#include "nifs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get a WASI file NIF by name
 * @param nifname the NIF name (e.g., "file:read_file/1")
 * @return pointer to the NIF structure, or NULL if not found
 */
const struct Nif *wasi_file_nifs_get_nif(const char *nifname);

#ifdef __cplusplus
}
#endif

#endif
