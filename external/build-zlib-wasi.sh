#!/usr/bin/env bash
set -euo pipefail

# Build zlib for WASI
# Based on: https://github.com/madler/zlib

WASI_SDK=${WASI_SDK_PATH:-"$HOME/wasi-sdk-22.0"}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ZLIB_SRC="$SCRIPT_DIR/zlib"
ZLIB_BUILD="$SCRIPT_DIR/zlib-wasi-build"
ZLIB_INSTALL="$SCRIPT_DIR/zlib-wasi"

echo "🔧 Building zlib for WASI..."
echo "  WASI SDK: $WASI_SDK"
echo "  Source: $ZLIB_SRC"
echo "  Build: $ZLIB_BUILD"
echo "  Install: $ZLIB_INSTALL"

# Clean previous build
rm -rf "$ZLIB_BUILD" "$ZLIB_INSTALL"
mkdir -p "$ZLIB_BUILD" "$ZLIB_INSTALL"

cd "$ZLIB_SRC"

# Configure for WASI
export CC="$WASI_SDK/bin/clang"
export AR="$WASI_SDK/bin/llvm-ar"
export RANLIB="$WASI_SDK/bin/llvm-ranlib"
export CFLAGS="--target=wasm32-wasi --sysroot=$WASI_SDK/share/wasi-sysroot -O2"

# Configure
./configure --prefix="$ZLIB_INSTALL" --static

# Build
make clean
make -j$(nproc)

# Install
make install

echo "✓ zlib built successfully!"
echo "  Headers: $ZLIB_INSTALL/include"
echo "  Library: $ZLIB_INSTALL/lib"
ls -lh "$ZLIB_INSTALL/lib/libz.a"
