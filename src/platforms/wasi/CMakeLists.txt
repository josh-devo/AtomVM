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
    src/lib/sys_wasi.c
    src/lib/time_wasi.c
    src/lib/memory_wasi.c
    src/lib/random_wasi.c
)

# Create platform library
add_library(atomvm_platform_wasi STATIC ${WASI_PLATFORM_SOURCES})

target_include_directories(atomvm_platform_wasi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/libAtomVM
)

target_link_libraries(atomvm_platform_wasi PUBLIC
    libAtomVM
)

# Main executable
add_executable(AtomVM.wasm
    src/main.c
)

target_link_libraries(AtomVM.wasm
    libAtomVM
    atomvm_platform_wasi
)

# WASI-specific compiler flags
target_compile_definitions(AtomVM.wasm PRIVATE
    AVM_PLATFORM_WASI
    AVM_NO_SMP
    __wasi__
)

target_compile_options(AtomVM.wasm PRIVATE
    -O2
    -fno-exceptions
)

# WASI-specific linker flags
set_target_properties(AtomVM.wasm PROPERTIES
    LINK_FLAGS "-Wl,--allow-undefined -Wl,--export-dynamic -Wl,--no-entry -Wl,--export=main -Wl,--export=_start"
    OUTPUT_NAME "AtomVM"
    SUFFIX ".wasm"
)

# Install target
install(TARGETS AtomVM.wasm DESTINATION bin)
