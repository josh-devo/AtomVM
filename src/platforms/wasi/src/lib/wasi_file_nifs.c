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

/**
 * @file wasi_file_nifs.c
 * @brief File I/O NIFs for WASI platform using WASI filesystem APIs
 */

#include "wasi_file_nifs.h"

#include "atom.h"
#include "context.h"
#include "defaultatoms.h"
#include "globalcontext.h"
#include "interop.h"
#include "memory.h"
#include "nifs.h"
#include "term.h"

#include <wasi/wasip2.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define TRACE_FILE(...)
// #define TRACE_FILE printf

// Get the first preopened directory (usually current directory)
static filesystem_own_descriptor_t get_preopen_dir(void)
{
    filesystem_preopens_list_tuple2_own_descriptor_string_t preopens;
    filesystem_preopens_get_directories(&preopens);

    if (preopens.len == 0) {
        TRACE_FILE("wasi_file: No preopened directories available\n");
        return (filesystem_own_descriptor_t){ .__handle = -1 };
    }

    // Return the first preopened directory
    filesystem_own_descriptor_t dir = preopens.ptr[0].f0;

    // Free the list but not the descriptor we're returning
    for (size_t i = 1; i < preopens.len; i++) {
        filesystem_descriptor_drop_own(preopens.ptr[i].f0);
    }
    free(preopens.ptr);

    return dir;
}

// Convert WASI filesystem error to Erlang atom
static term wasi_fs_error_to_atom(filesystem_error_code_t err, GlobalContext *glb)
{
    switch (err) {
        case FILESYSTEM_ERROR_CODE_ACCESS:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "eacces"));
        case FILESYSTEM_ERROR_CODE_WOULD_BLOCK:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "eagain"));
        case FILESYSTEM_ERROR_CODE_ALREADY:
            return globalcontext_make_atom(glb, ATOM_STR("\x7", "ealready"));
        case FILESYSTEM_ERROR_CODE_BAD_DESCRIPTOR:
            return globalcontext_make_atom(glb, ATOM_STR("\x5", "ebadf"));
        case FILESYSTEM_ERROR_CODE_BUSY:
            return globalcontext_make_atom(glb, ATOM_STR("\x5", "ebusy"));
        case FILESYSTEM_ERROR_CODE_EXIST:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "eexist"));
        case FILESYSTEM_ERROR_CODE_FILE_TOO_LARGE:
            return globalcontext_make_atom(glb, ATOM_STR("\x5", "efbig"));
        case FILESYSTEM_ERROR_CODE_INTERRUPTED:
            return globalcontext_make_atom(glb, ATOM_STR("\x5", "eintr"));
        case FILESYSTEM_ERROR_CODE_INVALID:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "einval"));
        case FILESYSTEM_ERROR_CODE_IO:
            return globalcontext_make_atom(glb, ATOM_STR("\x3", "eio"));
        case FILESYSTEM_ERROR_CODE_IS_DIRECTORY:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "eisdir"));
        case FILESYSTEM_ERROR_CODE_LOOP:
            return globalcontext_make_atom(glb, ATOM_STR("\x5", "eloop"));
        case FILESYSTEM_ERROR_CODE_TOO_MANY_LINKS:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "emlink"));
        case FILESYSTEM_ERROR_CODE_NAME_TOO_LONG:
            return globalcontext_make_atom(glb, ATOM_STR("\xb", "enametoolong"));
        case FILESYSTEM_ERROR_CODE_NO_DEVICE:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "enodev"));
        case FILESYSTEM_ERROR_CODE_NO_ENTRY:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "enoent"));
        case FILESYSTEM_ERROR_CODE_INSUFFICIENT_SPACE:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "enospc"));
        case FILESYSTEM_ERROR_CODE_NOT_DIRECTORY:
            return globalcontext_make_atom(glb, ATOM_STR("\x8", "enotdir"));
        case FILESYSTEM_ERROR_CODE_NOT_EMPTY:
            return globalcontext_make_atom(glb, ATOM_STR("\x9", "enotempty"));
        case FILESYSTEM_ERROR_CODE_UNSUPPORTED:
            return globalcontext_make_atom(glb, ATOM_STR("\x9", "enotsup"));
        case FILESYSTEM_ERROR_CODE_READ_ONLY:
            return globalcontext_make_atom(glb, ATOM_STR("\x5", "erofs"));
        default:
            return globalcontext_make_atom(glb, ATOM_STR("\x7", "unknown"));
    }
}

// file:write_file/2
static term nif_file_write_file(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    // Get filename
    int ok;
    char *filename = interop_term_to_string(argv[0], &ok);
    if (!ok) {
        TRACE_FILE("wasi_file: write_file - invalid filename\n");
        return ERROR_ATOM;
    }

    // Get binary data to write
    term data_term = argv[1];
    if (!term_is_binary(data_term)) {
        free(filename);
        TRACE_FILE("wasi_file: write_file - data is not binary\n");
        return ERROR_ATOM;
    }

    const uint8_t *data = (const uint8_t *) term_binary_data(data_term);
    size_t data_len = term_binary_size(data_term);

    TRACE_FILE("wasi_file: write_file - file=%s, size=%zu\n", filename, data_len);

    // Get preopened directory
    filesystem_own_descriptor_t dir = get_preopen_dir();
    if (dir.__handle == -1) {
        free(filename);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x6", "enoent")));
        return error_tuple;
    }

    // Prepare filename string
    wasip2_string_t path = {
        .ptr = (uint8_t *)filename,
        .len = strlen(filename)
    };

    // Open file for writing (create if doesn't exist, truncate if exists)
    filesystem_open_flags_t open_flags = FILESYSTEM_OPEN_FLAGS_CREATE | FILESYSTEM_OPEN_FLAGS_TRUNCATE;
    filesystem_descriptor_flags_t desc_flags = FILESYSTEM_DESCRIPTOR_FLAGS_WRITE;
    filesystem_path_flags_t path_flags = 0;

    filesystem_own_descriptor_t file_desc;
    filesystem_error_code_t err;

    bool result = filesystem_method_descriptor_open_at(
        filesystem_borrow_descriptor(dir),
        path_flags,
        &path,
        open_flags,
        desc_flags,
        &file_desc,
        &err
    );

    if (!result) {
        TRACE_FILE("wasi_file: write_file - open failed: %d\n", err);
        filesystem_descriptor_drop_own(dir);
        free(filename);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    // Write data directly (not via stream for simplicity)
    wasip2_list_u8_t write_buf = {
        .ptr = (uint8_t *)data,
        .len = data_len
    };

    filesystem_filesize_t bytes_written;
    result = filesystem_method_descriptor_write(
        filesystem_borrow_descriptor(file_desc),
        &write_buf,
        0,  // offset
        &bytes_written,
        &err
    );

    filesystem_descriptor_drop_own(file_desc);
    filesystem_descriptor_drop_own(dir);
    free(filename);

    if (!result) {
        TRACE_FILE("wasi_file: write_file - write failed: %d\n", err);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    TRACE_FILE("wasi_file: write_file - success, wrote %llu bytes\n", bytes_written);
    return OK_ATOM;
}

// file:read_file/1
static term nif_file_read_file(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    // Get filename
    int ok;
    char *filename = interop_term_to_string(argv[0], &ok);
    if (!ok) {
        TRACE_FILE("wasi_file: read_file - invalid filename\n");
        return ERROR_ATOM;
    }

    TRACE_FILE("wasi_file: read_file - file=%s\n", filename);

    // Get preopened directory
    filesystem_own_descriptor_t dir = get_preopen_dir();
    if (dir.__handle == -1) {
        free(filename);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x6", "enoent")));
        return error_tuple;
    }

    // Prepare filename string
    wasip2_string_t path = {
        .ptr = (uint8_t *)filename,
        .len = strlen(filename)
    };

    // Open file for reading
    filesystem_open_flags_t open_flags = 0;
    filesystem_descriptor_flags_t desc_flags = FILESYSTEM_DESCRIPTOR_FLAGS_READ;
    filesystem_path_flags_t path_flags = 0;

    filesystem_own_descriptor_t file_desc;
    filesystem_error_code_t err;

    bool result = filesystem_method_descriptor_open_at(
        filesystem_borrow_descriptor(dir),
        path_flags,
        &path,
        open_flags,
        desc_flags,
        &file_desc,
        &err
    );

    if (!result) {
        TRACE_FILE("wasi_file: read_file - open failed: %d\n", err);
        filesystem_descriptor_drop_own(dir);
        free(filename);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    // Get file size first
    filesystem_descriptor_stat_t stat;
    result = filesystem_method_descriptor_stat(
        filesystem_borrow_descriptor(file_desc),
        &stat,
        &err
    );

    if (!result) {
        TRACE_FILE("wasi_file: read_file - stat failed: %d\n", err);
        filesystem_descriptor_drop_own(file_desc);
        filesystem_descriptor_drop_own(dir);
        free(filename);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    filesystem_filesize_t file_size = stat.size;
    TRACE_FILE("wasi_file: read_file - file size=%llu\n", file_size);

    // Read entire file
    wasip2_tuple2_list_u8_bool_t read_result;
    result = filesystem_method_descriptor_read(
        filesystem_borrow_descriptor(file_desc),
        file_size,  // length to read
        0,          // offset
        &read_result,
        &err
    );

    filesystem_descriptor_drop_own(file_desc);
    filesystem_descriptor_drop_own(dir);
    free(filename);

    if (!result) {
        TRACE_FILE("wasi_file: read_file - read failed: %d\n", err);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    // Create binary from read data
    term bin_term = term_from_literal_binary(
        read_result.f0.ptr,
        read_result.f0.len,
        &ctx->heap,
        glb
    );

    // Free the read buffer using WASI cleanup function
    wasip2_list_u8_free(&read_result.f0);

    // Return {ok, Binary}
    term ok_tuple = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(ok_tuple, 0, OK_ATOM);
    term_put_tuple_element(ok_tuple, 1, bin_term);

    TRACE_FILE("wasi_file: read_file - success, read %zu bytes\n", read_result.f0.len);
    return ok_tuple;
}

// file:read_file_info/1
static term nif_file_read_file_info(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    // Get filename
    int ok;
    char *filename = interop_term_to_string(argv[0], &ok);
    if (!ok) {
        TRACE_FILE("wasi_file: read_file_info - invalid filename\n");
        return ERROR_ATOM;
    }

    TRACE_FILE("wasi_file: read_file_info - file=%s\n", filename);

    // Get preopened directory
    filesystem_own_descriptor_t dir = get_preopen_dir();
    if (dir.__handle == -1) {
        free(filename);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x6", "enoent")));
        return error_tuple;
    }

    // Prepare filename string
    wasip2_string_t path = {
        .ptr = (uint8_t *)filename,
        .len = strlen(filename)
    };

    // Get file stats
    filesystem_path_flags_t path_flags = 0;
    filesystem_descriptor_stat_t stat;
    filesystem_error_code_t err;

    bool result = filesystem_method_descriptor_stat_at(
        filesystem_borrow_descriptor(dir),
        path_flags,
        &path,
        &stat,
        &err
    );

    filesystem_descriptor_drop_own(dir);
    free(filename);

    if (!result) {
        TRACE_FILE("wasi_file: read_file_info - stat failed: %d\n", err);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    // Build file_info tuple: {Size, Type, ...}
    // For now, return a simple tuple with size and type
    term size_term = term_from_int64(stat.size);

    term type_atom;
    switch (stat.type) {
        case FILESYSTEM_DESCRIPTOR_TYPE_REGULAR_FILE:
            type_atom = globalcontext_make_atom(glb, ATOM_STR("\x7", "regular"));
            break;
        case FILESYSTEM_DESCRIPTOR_TYPE_DIRECTORY:
            type_atom = globalcontext_make_atom(glb, ATOM_STR("\x9", "directory"));
            break;
        case FILESYSTEM_DESCRIPTOR_TYPE_SYMBOLIC_LINK:
            type_atom = globalcontext_make_atom(glb, ATOM_STR("\x7", "symlink"));
            break;
        default:
            type_atom = globalcontext_make_atom(glb, ATOM_STR("\x5", "other"));
            break;
    }

    // Create a simple file_info tuple: {file_info, Size, Type}
    term file_info_tuple = term_alloc_tuple(3, &ctx->heap);
    term_put_tuple_element(file_info_tuple, 0, globalcontext_make_atom(glb, ATOM_STR("\x9", "file_info")));
    term_put_tuple_element(file_info_tuple, 1, size_term);
    term_put_tuple_element(file_info_tuple, 2, type_atom);

    // Return {ok, FileInfo}
    term ok_tuple = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(ok_tuple, 0, OK_ATOM);
    term_put_tuple_element(ok_tuple, 1, file_info_tuple);

    TRACE_FILE("wasi_file: read_file_info - success, size=%llu type=%d\n", stat.size, stat.type);
    return ok_tuple;
}

// file:delete/1
static term nif_file_delete(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    // Get filename
    int ok;
    char *filename = interop_term_to_string(argv[0], &ok);
    if (!ok) {
        TRACE_FILE("wasi_file: delete - invalid filename\n");
        return ERROR_ATOM;
    }

    TRACE_FILE("wasi_file: delete - attempting to delete %s\n", filename);

    // Get preopened directory
    filesystem_own_descriptor_t dir = get_preopen_dir();
    if (dir.__handle == -1) {
        free(filename);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x6", "enoent")));
        return error_tuple;
    }

    // Prepare filename string
    wasip2_string_t path = {
        .ptr = (uint8_t *)filename,
        .len = strlen(filename)
    };

    // Delete the file
    filesystem_error_code_t err;
    bool result = filesystem_method_descriptor_unlink_file_at(
        filesystem_borrow_descriptor(dir),
        &path,
        &err
    );

    free(filename);
    filesystem_descriptor_drop_own(dir);

    if (!result) {
        TRACE_FILE("wasi_file: delete failed - %d\n", err);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    TRACE_FILE("wasi_file: delete - success\n");
    return OK_ATOM;
}

// file:make_dir/1
static term nif_file_make_dir(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    // Get directory name
    int ok;
    char *dirname = interop_term_to_string(argv[0], &ok);
    if (!ok) {
        TRACE_FILE("wasi_file: make_dir - invalid dirname\n");
        return ERROR_ATOM;
    }

    TRACE_FILE("wasi_file: make_dir - attempting to create %s\n", dirname);

    // Get preopened directory
    filesystem_own_descriptor_t dir = get_preopen_dir();
    if (dir.__handle == -1) {
        free(dirname);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x6", "enoent")));
        return error_tuple;
    }

    // Prepare dirname string
    wasip2_string_t path = {
        .ptr = (uint8_t *)dirname,
        .len = strlen(dirname)
    };

    // Create the directory
    filesystem_error_code_t err;
    bool result = filesystem_method_descriptor_create_directory_at(
        filesystem_borrow_descriptor(dir),
        &path,
        &err
    );

    free(dirname);
    filesystem_descriptor_drop_own(dir);

    if (!result) {
        TRACE_FILE("wasi_file: make_dir failed - %d\n", err);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    TRACE_FILE("wasi_file: make_dir - success\n");
    return OK_ATOM;
}

// file:del_dir/1
static term nif_file_del_dir(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    // Get directory name
    int ok;
    char *dirname = interop_term_to_string(argv[0], &ok);
    if (!ok) {
        TRACE_FILE("wasi_file: del_dir - invalid dirname\n");
        return ERROR_ATOM;
    }

    TRACE_FILE("wasi_file: del_dir - attempting to remove %s\n", dirname);

    // Get preopened directory
    filesystem_own_descriptor_t dir = get_preopen_dir();
    if (dir.__handle == -1) {
        free(dirname);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x6", "enoent")));
        return error_tuple;
    }

    // Prepare dirname string
    wasip2_string_t path = {
        .ptr = (uint8_t *)dirname,
        .len = strlen(dirname)
    };

    // Remove the directory
    filesystem_error_code_t err;
    bool result = filesystem_method_descriptor_remove_directory_at(
        filesystem_borrow_descriptor(dir),
        &path,
        &err
    );

    free(dirname);
    filesystem_descriptor_drop_own(dir);

    if (!result) {
        TRACE_FILE("wasi_file: del_dir failed - %d\n", err);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    TRACE_FILE("wasi_file: del_dir - success\n");
    return OK_ATOM;
}

// file:rename/2
static term nif_file_rename(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    // Get old filename
    int ok;
    char *old_name = interop_term_to_string(argv[0], &ok);
    if (!ok) {
        TRACE_FILE("wasi_file: rename - invalid old filename\n");
        return ERROR_ATOM;
    }

    // Get new filename
    char *new_name = interop_term_to_string(argv[1], &ok);
    if (!ok) {
        free(old_name);
        TRACE_FILE("wasi_file: rename - invalid new filename\n");
        return ERROR_ATOM;
    }

    TRACE_FILE("wasi_file: rename - attempting to rename %s to %s\n", old_name, new_name);

    // Get preopened directory
    filesystem_own_descriptor_t dir = get_preopen_dir();
    if (dir.__handle == -1) {
        free(old_name);
        free(new_name);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1,
            globalcontext_make_atom(glb, ATOM_STR("\x6", "enoent")));
        return error_tuple;
    }

    // Prepare old path string
    wasip2_string_t old_path = {
        .ptr = (uint8_t *)old_name,
        .len = strlen(old_name)
    };

    // Prepare new path string
    wasip2_string_t new_path = {
        .ptr = (uint8_t *)new_name,
        .len = strlen(new_name)
    };

    // Rename the file/directory
    filesystem_error_code_t err;
    bool result = filesystem_method_descriptor_rename_at(
        filesystem_borrow_descriptor(dir),
        &old_path,
        filesystem_borrow_descriptor(dir),  // Using same dir for destination
        &new_path,
        &err
    );

    free(old_name);
    free(new_name);
    filesystem_descriptor_drop_own(dir);

    if (!result) {
        TRACE_FILE("wasi_file: rename failed - %d\n", err);
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, wasi_fs_error_to_atom(err, glb));
        return error_tuple;
    }

    TRACE_FILE("wasi_file: rename - success\n");
    return OK_ATOM;
}

// file:list_dir/1
// TODO: Both WASI P2 directory listing APIs and POSIX readdir() have memory
// issues in current WASI implementations. This needs to be revisited when
// WASI tooling is more mature. For now, return enotsup.
static term nif_file_list_dir(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    GlobalContext *glb = ctx->global;

    TRACE_FILE("wasi_file: list_dir - not supported in current WASI implementation\n");

    // Return {error, enotsup}
    term error_tuple = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
    term_put_tuple_element(error_tuple, 1,
        globalcontext_make_atom(glb, ATOM_STR("\x7", "enotsup")));
    return error_tuple;
}

// NIF structures
static const struct Nif file_read_file_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_file_read_file
};

static const struct Nif file_write_file_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_file_write_file
};

static const struct Nif file_read_file_info_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_file_read_file_info
};

static const struct Nif file_delete_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_file_delete
};

static const struct Nif file_make_dir_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_file_make_dir
};

static const struct Nif file_del_dir_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_file_del_dir
};

static const struct Nif file_rename_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_file_rename
};

static const struct Nif file_list_dir_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_file_list_dir
};

const struct Nif *wasi_file_nifs_get_nif(const char *nifname)
{
    if (strcmp("file:read_file/1", nifname) == 0) {
        return &file_read_file_nif;
    }
    if (strcmp("file:write_file/2", nifname) == 0) {
        return &file_write_file_nif;
    }
    if (strcmp("file:read_file_info/1", nifname) == 0) {
        return &file_read_file_info_nif;
    }
    if (strcmp("file:delete/1", nifname) == 0) {
        return &file_delete_nif;
    }
    if (strcmp("file:make_dir/1", nifname) == 0) {
        return &file_make_dir_nif;
    }
    if (strcmp("file:del_dir/1", nifname) == 0) {
        return &file_del_dir_nif;
    }
    if (strcmp("file:rename/2", nifname) == 0) {
        return &file_rename_nif;
    }
    if (strcmp("file:list_dir/1", nifname) == 0) {
        return &file_list_dir_nif;
    }

    return NULL;
}
