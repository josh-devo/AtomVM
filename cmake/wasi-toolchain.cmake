# CMake Toolchain file for WASI
set(CMAKE_SYSTEM_NAME WASI)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

# WASI SDK paths
set(WASI_SDK_PREFIX "/home/joshadams/wasi-sdk-22.0")
set(CMAKE_C_COMPILER "${WASI_SDK_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${WASI_SDK_PREFIX}/bin/clang++")
set(CMAKE_AR "${WASI_SDK_PREFIX}/bin/llvm-ar")
set(CMAKE_RANLIB "${WASI_SDK_PREFIX}/bin/llvm-ranlib")
set(CMAKE_SYSROOT "${WASI_SDK_PREFIX}/share/wasi-sysroot")

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --target=wasm32-wasi")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=wasm32-wasi")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-undefined")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--export-dynamic")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-entry")

# Find programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Platform-specific definitions
add_compile_definitions(
    AVM_PLATFORM_WASI
    __wasi__
)
