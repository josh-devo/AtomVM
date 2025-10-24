# This file is part of AtomVM.
#
# Copyright 2025 AtomVM Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later

cmake_minimum_required(VERSION 3.20)

project(AtomVM_WASI)

# Platform source files
set(WASI_PLATFORM_SOURCES
    src/lib/platform_wasi.h
    src/lib/platform_defaultatoms.h
    src/lib/platform_defaultatoms.c
    src/lib/platform_defaultatoms.def
    src/lib/platform_nifs.c
    src/lib/sys_wasi.c
    src/lib/time_wasi.c
    src/lib/memory_wasi.c
    src/lib/random_wasi.c
    src/lib/wasi_net.h
    src/lib/wasi_net.c
    src/lib/wasi_file_nifs.h
    src/lib/wasi_file_nifs.c
)

# Find mbedtls
find_library(MBEDTLS_LIBRARY mbedtls)
find_library(MBEDX509_LIBRARY mbedx509)
find_library(MBEDCRYPTO_LIBRARY mbedcrypto)

# Platform library suffix to match expected naming
set(PLATFORM_LIB_SUFFIX ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})

# Create platform library
add_library(libAtomVM${PLATFORM_LIB_SUFFIX} STATIC ${WASI_PLATFORM_SOURCES})

target_include_directories(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/libAtomVM
    ${CMAKE_BINARY_DIR}/src/libAtomVM  # For avm_version.h
)

# Force WASI-specific defines for the platform library
target_compile_definitions(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC
    AVM_PLATFORM_WASI
    AVM_NO_SMP
    AVM_NO_JIT
    __wasi__
    __wasilibc_use_wasip2
)

target_link_libraries(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC
    libAtomVM
)

# Link mbedtls if found, otherwise we'll use WASI random directly
if(MBEDTLS_LIBRARY)
    target_link_libraries(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC
        ${MBEDTLS_LIBRARY}
        ${MBEDX509_LIBRARY}
        ${MBEDCRYPTO_LIBRARY}
    )
    target_compile_definitions(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC HAVE_MBEDTLS)
endif()

# Add zlib support for BEAM decompression
set(ZLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/zlib-wasi/include")
set(ZLIB_LIBRARY "${CMAKE_SOURCE_DIR}/external/zlib-wasi/lib/libz.a")

if(EXISTS "${ZLIB_LIBRARY}")
    message(STATUS "Found zlib for WASI: ${ZLIB_LIBRARY}")
    set(ZLIB_FOUND TRUE)
else()
    message(WARNING "zlib not found for WASI. Run: bash external/build-zlib-wasi.sh")
    set(ZLIB_FOUND FALSE)
endif()

# Add libAtomVM subdirectory (needed for avm_version.h generation)
add_subdirectory(../../libAtomVM libAtomVM)

# Enable zlib support in libAtomVM for WASI
if(ZLIB_FOUND)
    target_compile_definitions(libAtomVM PUBLIC WITH_ZLIB)
    target_include_directories(libAtomVM PUBLIC ${ZLIB_INCLUDE_DIR})
    target_link_libraries(libAtomVM PUBLIC ${ZLIB_LIBRARY})
endif()

# Main executable
add_executable(AtomVM.wasm
    src/main.c
)

target_include_directories(AtomVM.wasm PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/libAtomVM  # For avm_version.h
)

target_link_libraries(AtomVM.wasm
    libAtomVM
)

# WASI-specific compiler flags (already set on platform library, but repeat for clarity)
target_compile_definitions(AtomVM.wasm PRIVATE
    AVM_PLATFORM_WASI
    AVM_NO_SMP
    AVM_NO_JIT
    __wasi__
)

target_compile_options(AtomVM.wasm PRIVATE
    -O2
    -fno-exceptions
)

# WASI-specific linker flags - adjusted for wasm32-wasip2 component model
set_target_properties(AtomVM.wasm PROPERTIES
    LINK_FLAGS "-Wl,--no-entry -Wl,--export=main -Wl,--export=_start"
    OUTPUT_NAME "AtomVM"
    SUFFIX ".wasm"
)

# Install target
install(TARGETS AtomVM.wasm DESTINATION bin)
