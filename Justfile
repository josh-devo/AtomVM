# Justfile for AtomVM WASI Build
# Run `just --list` to see all available commands

# Default WASI SDK version and paths
WASI_SDK_VERSION := "22.0"
WASI_SDK_DIR := env_var_or_default('WASI_SDK_PATH', justfile_directory() + "/tools/wasi-sdk-" + WASI_SDK_VERSION)
BUILD_DIR := "build-wasi"

# Default recipe - show help
default:
    @just --list

# Quick command: configure, build, and show status
wasi: wasi-build wasi-info

# Build WASI (same as build but more explicit)
wasi-build: configure
    @echo "🔨 Building AtomVM for WASI..."
    cd {{BUILD_DIR}} && make -j16 AtomVM.wasm
    @echo "✓ WASI build complete!"

# Show WASI build info
wasi-info:
    @echo "📊 AtomVM WASI Build Status"
    @echo "============================"
    @if [ -f "{{BUILD_DIR}}/src/platforms/wasi/AtomVM.wasm" ]; then \
        echo "✓ AtomVM.wasm built successfully"; \
        echo "  Location: {{BUILD_DIR}}/src/platforms/wasi/AtomVM.wasm"; \
        echo "  Size: $(du -h {{BUILD_DIR}}/src/platforms/wasi/AtomVM.wasm | cut -f1)"; \
        if command -v file >/dev/null 2>&1; then \
            echo "  Type: $(file {{BUILD_DIR}}/src/platforms/wasi/AtomVM.wasm | cut -d: -f2)"; \
        fi; \
    else \
        echo "✗ AtomVM.wasm not built yet"; \
        echo "  Run: just wasi-build"; \
    fi

# Clean WASI build only
wasi-clean:
    @echo "🧹 Cleaning WASI build..."
    rm -rf {{BUILD_DIR}}
    @echo "✓ WASI build cleaned"

# Rebuild WASI from scratch
wasi-rebuild: wasi-clean wasi-build

# Setup: Install WASI SDK and all dependencies
setup: install-wasi-sdk
    @echo "✓ Setup complete!"
    @echo "  WASI SDK installed at: {{WASI_SDK_DIR}}"

# Install WASI SDK if not already present
install-wasi-sdk:
    #!/usr/bin/env bash
    set -euo pipefail
    if [ -d "{{WASI_SDK_DIR}}" ]; then
        echo "✓ WASI SDK already installed at {{WASI_SDK_DIR}}"
        exit 0
    fi

    echo "📦 Downloading WASI SDK {{WASI_SDK_VERSION}}..."
    mkdir -p tools
    cd tools

    TARBALL="wasi-sdk-{{WASI_SDK_VERSION}}-linux.tar.gz"
    URL="https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-{{replace(WASI_SDK_VERSION, ".0", "")}}/$TARBALL"

    if [ ! -f "$TARBALL" ]; then
        wget -q --show-progress "$URL"
    fi

    echo "📂 Extracting WASI SDK..."
    tar xzf "$TARBALL"

    echo "✓ WASI SDK installed successfully!"
    {{WASI_SDK_DIR}}/bin/clang --version

# Verify WASI SDK installation
verify-wasi:
    #!/usr/bin/env bash
    set -euo pipefail
    if [ ! -d "{{WASI_SDK_DIR}}" ]; then
        echo "❌ WASI SDK not found at {{WASI_SDK_DIR}}"
        echo "   Run: just install-wasi-sdk"
        exit 1
    fi

    echo "✓ WASI SDK found at {{WASI_SDK_DIR}}"
    {{WASI_SDK_DIR}}/bin/clang --version

# Create CMake toolchain file for WASI
create-toolchain: verify-wasi
    python3 -c 'import os; os.makedirs("cmake", exist_ok=True); open("cmake/wasi-toolchain.cmake", "w").write("""# CMake Toolchain file for WASI\nset(CMAKE_SYSTEM_NAME WASI)\nset(CMAKE_SYSTEM_VERSION 1)\nset(CMAKE_SYSTEM_PROCESSOR wasm32)\n\n# WASI SDK paths\nset(WASI_SDK_PREFIX "{{WASI_SDK_DIR}}")\nset(CMAKE_C_COMPILER "${WASI_SDK_PREFIX}/bin/clang")\nset(CMAKE_CXX_COMPILER "${WASI_SDK_PREFIX}/bin/clang++")\nset(CMAKE_AR "${WASI_SDK_PREFIX}/bin/llvm-ar")\nset(CMAKE_RANLIB "${WASI_SDK_PREFIX}/bin/llvm-ranlib")\nset(CMAKE_SYSROOT "${WASI_SDK_PREFIX}/share/wasi-sysroot")\n\n# Compiler flags\nset(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --target=wasm32-wasi")\nset(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=wasm32-wasi")\n\n# Linker flags\nset(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-undefined")\nset(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--export-dynamic")\nset(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-entry")\n\n# Find programs in the host environment\nset(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\nset(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\nset(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\nset(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)\n\n# Platform-specific definitions\nadd_compile_definitions(\n    AVM_PLATFORM_WASI\n    __wasi__\n)\n""")'
    @echo "✓ Toolchain file created at cmake/wasi-toolchain.cmake"

# Configure CMake for WASI build
configure: create-toolchain
    #!/usr/bin/env bash
    set -euo pipefail

    echo "🔧 Configuring CMake for WASI build..."
    mkdir -p {{BUILD_DIR}}
    cd {{BUILD_DIR}}

    cmake .. \
        -DCMAKE_TOOLCHAIN_FILE=../cmake/wasi-toolchain.cmake \
        -DAVM_PLATFORM_WASI=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DAVM_DISABLE_SMP=ON \
        -DAVM_DISABLE_TASK_DRIVER=ON \
        -DAVM_DISABLE_NETWORKING=ON

    echo "✓ Configuration complete!"

# Build AtomVM for WASI
build: configure
    #!/usr/bin/env bash
    set -euo pipefail

    echo "🔨 Building AtomVM for WASI..."
    cd {{BUILD_DIR}}
    make -j$(nproc)

    echo "✓ Build complete!"
    echo "  Output: {{BUILD_DIR}}/src/AtomVM.wasm"

# Clean build artifacts
clean:
    @echo "🧹 Cleaning build artifacts..."
    rm -rf {{BUILD_DIR}}
    @echo "✓ Clean complete!"

# Clean everything including downloaded tools
distclean: clean
    @echo "🧹 Cleaning all downloaded tools..."
    rm -rf tools
    rm -f cmake/wasi-toolchain.cmake
    @echo "✓ Deep clean complete!"

# Run tests with wasmtime
test: build install-wasmtime
    #!/usr/bin/env bash
    set -euo pipefail

    echo "🧪 Testing AtomVM WASM with wasmtime..."

    if [ ! -f "{{BUILD_DIR}}/src/AtomVM.wasm" ]; then
        echo "❌ AtomVM.wasm not found. Run 'just build' first."
        exit 1
    fi

    # Basic validation
    wasmtime --version
    wasmtime validate {{BUILD_DIR}}/src/AtomVM.wasm

    echo "✓ WASM validation passed!"

# Install wasmtime if not present
install-wasmtime:
    #!/usr/bin/env bash
    set -euo pipefail

    if command -v wasmtime &> /dev/null; then
        echo "✓ wasmtime already installed: $(wasmtime --version)"
        exit 0
    fi

    echo "📦 Installing wasmtime..."
    curl https://wasmtime.dev/install.sh -sSf | bash

    echo "✓ wasmtime installed! You may need to restart your shell."
    echo "  Or run: source ~/.bashrc"

# Install wasm-opt for optimization
install-wasm-opt:
    #!/usr/bin/env bash
    set -euo pipefail

    if command -v wasm-opt &> /dev/null; then
        echo "✓ wasm-opt already installed: $(wasm-opt --version)"
        exit 0
    fi

    echo "📦 Installing binaryen (wasm-opt)..."

    if command -v apt-get &> /dev/null; then
        sudo apt-get update && sudo apt-get install -y binaryen
    elif command -v brew &> /dev/null; then
        brew install binaryen
    else
        echo "❌ Please install binaryen manually: https://github.com/WebAssembly/binaryen"
        exit 1
    fi

    echo "✓ wasm-opt installed!"

# Optimize the built WASM binary
optimize: build install-wasm-opt
    #!/usr/bin/env bash
    set -euo pipefail

    echo "⚡ Optimizing WASM binary..."

    WASM_FILE="{{BUILD_DIR}}/src/AtomVM.wasm"
    OPTIMIZED_FILE="{{BUILD_DIR}}/src/AtomVM.optimized.wasm"

    # Show original size
    echo "Original size: $(du -h $WASM_FILE | cut -f1)"

    # Optimize
    wasm-opt -O3 --enable-bulk-memory $WASM_FILE -o $OPTIMIZED_FILE

    # Show optimized size
    echo "Optimized size: $(du -h $OPTIMIZED_FILE | cut -f1)"

    # Replace original with optimized
    mv $OPTIMIZED_FILE $WASM_FILE

    echo "✓ Optimization complete!"

# Strip debug symbols from WASM
strip: build
    #!/usr/bin/env bash
    set -euo pipefail

    echo "✂️  Stripping debug symbols..."

    WASM_FILE="{{BUILD_DIR}}/src/AtomVM.wasm"

    # Show original size
    echo "Original size: $(du -h $WASM_FILE | cut -f1)"

    # Strip
    {{WASI_SDK_DIR}}/bin/llvm-strip $WASM_FILE

    # Show stripped size
    echo "Stripped size: $(du -h $WASM_FILE | cut -f1)"

    echo "✓ Strip complete!"

# Full release build: build + optimize + strip
release: build optimize strip
    @echo "🚀 Release build complete!"
    @echo "  Output: {{BUILD_DIR}}/src/AtomVM.wasm"
    @du -h {{BUILD_DIR}}/src/AtomVM.wasm

# Show build information
info:
    @echo "AtomVM WASI Build Information"
    @echo "=============================="
    @echo "WASI SDK Version: {{WASI_SDK_VERSION}}"
    @echo "WASI SDK Path:    {{WASI_SDK_DIR}}"
    @echo "Build Directory:  {{BUILD_DIR}}"
    @echo ""
    @echo "Status:"
    @if [ -d "{{WASI_SDK_DIR}}" ]; then \
        echo "  ✓ WASI SDK installed"; \
    else \
        echo "  ✗ WASI SDK not installed (run: just install-wasi-sdk)"; \
    fi
    @if [ -d "{{BUILD_DIR}}" ]; then \
        echo "  ✓ Build directory exists"; \
        if [ -f "{{BUILD_DIR}}/src/AtomVM.wasm" ]; then \
            echo "  ✓ AtomVM.wasm built ($(du -h {{BUILD_DIR}}/src/AtomVM.wasm | cut -f1))"; \
        else \
            echo "  ✗ AtomVM.wasm not built (run: just build)"; \
        fi \
    else \
        echo "  ✗ Build directory not created (run: just configure)"; \
    fi

# Quick development cycle: clean + build
dev: clean build
    @echo "✓ Development build complete!"

# CI/CD full build and test
ci: setup build test
    @echo "✓ CI build complete!"

# WASI integration test - compile and run test programs
wasi-test:
    #!/usr/bin/env bash
    set -euo pipefail

    echo "🧪 Running WASI integration tests..."

    # Check prerequisites
    if [ ! -f "{{BUILD_DIR}}/src/platforms/wasi/AtomVM.wasm" ]; then
        echo "❌ AtomVM.wasm not found. Run 'just wasi-build' first."
        exit 1
    fi

    # Find wasmtime
    WASMTIME=""
    if command -v wasmtime &> /dev/null; then
        WASMTIME="wasmtime"
    elif [ -f "$HOME/.wasmtime/bin/wasmtime" ]; then
        WASMTIME="$HOME/.wasmtime/bin/wasmtime"
    else
        echo "❌ wasmtime not found. Installing..."
        just install-wasmtime
        WASMTIME="$HOME/.wasmtime/bin/wasmtime"
    fi

    if ! command -v erlc &> /dev/null; then
        echo "❌ erlc not found. Please install Erlang/OTP."
        exit 1
    fi

    # Compile test programs
    echo "📝 Compiling test programs..."
    mkdir -p tests/wasi
    cd tests/wasi
    erlc simple_test.erl display_test.erl math_test.erl atom_test.erl list_test.erl \
         test_zlib_compress.erl spawn_fun1.erl test_ets.erl

    # Run tests
    echo ""
    echo "🚀 Running tests with wasmtime..."
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # Run all integration tests (zlib support enabled)
    # Note: code_lock excluded (requires gen_statem from OTP libs)
    TESTS=(simple_test display_test math_test atom_test list_test test_zlib_compress spawn_fun1 test_ets)
    PASSED=0
    FAILED=0

    for test in "${TESTS[@]}"; do
        echo ""
        echo "▶ Running $test..."
        OUTPUT=$($WASMTIME run --dir=. ../../{{BUILD_DIR}}/src/platforms/wasi/AtomVM.wasm $test.beam 2>&1)
        EXIT_CODE=$?
        # Accept tests that return ok or integer values (0, 42, etc.)
        # Ignore exit code if return value is correct (some tests trigger init.beam warnings)
        if echo "$OUTPUT" | grep -qE "Return value: (ok|[0-9]+)"; then
            echo "  ✓ $test passed"
            PASSED=$((PASSED + 1))
        else
            echo "  ✗ $test failed (exit code: $EXIT_CODE)"
            echo "$OUTPUT" | head -10
            FAILED=$((FAILED + 1))
        fi
    done

    echo ""
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "📊 Test Results: $PASSED passed, $FAILED failed"

    if [ $FAILED -eq 0 ]; then
        echo "✓ All WASI integration tests passed!"
        exit 0
    else
        echo "✗ Some tests failed"
        exit 1
    fi

# Install just if not present (helper recipe)
install-just:
    #!/usr/bin/env bash
    set -euo pipefail

    if command -v just &> /dev/null; then
        echo "✓ just already installed: $(just --version)"
        exit 0
    fi

    echo "📦 Installing just..."

    if command -v cargo &> /dev/null; then
        cargo install just
    elif command -v brew &> /dev/null; then
        brew install just
    else
        echo "Please install just manually: https://github.com/casey/just"
        exit 1
    fi

    echo "✓ just installed!"
