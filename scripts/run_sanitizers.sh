#!/bin/bash
# run_sanitizers.sh - Run tests with AddressSanitizer and UBSan

set -e

BUILD_DIR="${1:-build-sanitizers}"

echo "=== Building with Sanitizers ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with sanitizers
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DBUILD_IPC_GATEWAY=ON

# Build
echo ""
echo "=== Building ==="
cmake --build . --target \
  ipc-protocol-test \
  ipc-config-test \
  ipc-backpressure-test \
  json-validator-test \
  ipc-capabilities-test \
  fuzz-ipc-protocol

# Run tests
echo ""
echo "=== Running Tests with Sanitizers ==="

export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

echo ""
echo "1. Protocol test..."
./ipc-protocol-test

echo ""
echo "2. Config test..."
./ipc-config-test

echo ""
echo "3. Backpressure test..."
./ipc-backpressure-test

echo ""
echo "4. JSON validator test..."
./json-validator-test

echo ""
echo "5. Capabilities test..."
./ipc-capabilities-test

echo ""
echo "6. Fuzz test (1000 iterations)..."
./fuzz-ipc-protocol 1000

echo ""
echo "=== ✅ All tests passed with sanitizers! ==="
echo "No memory leaks, no undefined behavior detected."
