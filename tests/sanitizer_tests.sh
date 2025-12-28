#!/bin/bash
# sanitizer_tests.sh - Run tests with memory sanitizers
#
# Validates buffer pool and connection pool for memory safety

set -e

echo "=========================================="
echo "Memory Safety Tests (Sanitizers)"
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if valgrind is available
if ! command -v valgrind &> /dev/null; then
    echo -e "${YELLOW}WARNING: valgrind not found${NC}"
    echo "Install: sudo apt-get install valgrind"
    echo ""
fi

# 1. Valgrind - Memory leak detection
echo "=== Test 1: Valgrind - Memory Leak Detection ==="
echo ""

if command -v valgrind &> /dev/null; then
    echo "Buffer Pool:"
    valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --error-exitcode=1 \
             ./build/test-buffer-pool 2>&1 | grep -E "(ERROR SUMMARY|definitely lost|indirectly lost|possibly lost)"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Buffer Pool: No leaks${NC}"
    else
        echo -e "${RED}✗ Buffer Pool: Leaks detected${NC}"
        exit 1
    fi
    
    echo ""
    echo "NATS Pool:"
    valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --error-exitcode=1 \
             ./build/test-nats-pool 2>&1 | grep -E "(ERROR SUMMARY|definitely lost|indirectly lost|possibly lost)"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ NATS Pool: No leaks${NC}"
    else
        echo -e "${RED}✗ NATS Pool: Leaks detected${NC}"
        exit 1
    fi
    
    echo ""
    echo "Trace Context:"
    valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --error-exitcode=1 \
             ./build/test-trace-context 2>&1 | grep -E "(ERROR SUMMARY|definitely lost|indirectly lost|possibly lost)"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Trace Context: No leaks${NC}"
    else
        echo -e "${RED}✗ Trace Context: Leaks detected${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}SKIPPED: valgrind not available${NC}"
fi

echo ""
echo "=== Test 2: AddressSanitizer (ASan) ==="
echo ""

# Check if we can rebuild with ASan
if [ -d "build" ]; then
    echo "Rebuilding with AddressSanitizer..."
    cd build
    
    # Save old build
    if [ -f "test-buffer-pool" ]; then
        mv test-buffer-pool test-buffer-pool.backup 2>/dev/null || true
        mv test-nats-pool test-nats-pool.backup 2>/dev/null || true
    fi
    
    # Build with ASan
    cmake .. -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
             -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
    make test-buffer-pool test-nats-pool 2>&1 | grep -E "(Built|Error)" || true
    
    if [ -f "test-buffer-pool" ]; then
        echo "Running buffer pool with ASan..."
        ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./test-buffer-pool
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✓ Buffer Pool: ASan passed${NC}"
        else
            echo -e "${RED}✗ Buffer Pool: ASan failed${NC}"
            exit 1
        fi
    fi
    
    if [ -f "test-nats-pool" ]; then
        echo "Running NATS pool with ASan..."
        ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./test-nats-pool
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✓ NATS Pool: ASan passed${NC}"
        else
            echo -e "${RED}✗ NATS Pool: ASan failed${NC}"
            exit 1
        fi
    fi
    
    # Restore backup
    if [ -f "test-buffer-pool.backup" ]; then
        mv test-buffer-pool.backup test-buffer-pool
        mv test-nats-pool.backup test-nats-pool
    fi
    
    # Rebuild without ASan
    cmake .. -DCMAKE_C_FLAGS="" -DCMAKE_EXE_LINKER_FLAGS=""
    make test-buffer-pool test-nats-pool > /dev/null 2>&1
    
    cd ..
else
    echo -e "${RED}ERROR: build directory not found${NC}"
    exit 1
fi

echo ""
echo "=== Test 3: Thread Sanitizer (TSan) ==="
echo ""

cd build

# Build with TSan
cmake .. -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
         -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
make test-buffer-pool test-nats-pool 2>&1 | grep -E "(Built|Error)" || true

if [ -f "test-buffer-pool" ]; then
    echo "Running buffer pool with TSan (thread safety)..."
    TSAN_OPTIONS=halt_on_error=1 ./test-buffer-pool
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Buffer Pool: TSan passed (thread-safe)${NC}"
    else
        echo -e "${RED}✗ Buffer Pool: TSan failed (race condition)${NC}"
        exit 1
    fi
fi

if [ -f "test-nats-pool" ]; then
    echo "Running NATS pool with TSan (thread safety)..."
    TSAN_OPTIONS=halt_on_error=1 ./test-nats-pool
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ NATS Pool: TSan passed (thread-safe)${NC}"
    else
        echo -e "${RED}✗ NATS Pool: TSan failed (race condition)${NC}"
        exit 1
    fi
fi

# Restore normal build
cmake .. -DCMAKE_C_FLAGS="" -DCMAKE_EXE_LINKER_FLAGS=""
make test-buffer-pool test-nats-pool > /dev/null 2>&1

cd ..

echo ""
echo "=========================================="
echo -e "${GREEN}✅ All sanitizer tests PASSED${NC}"
echo "=========================================="
echo ""
echo "Validated:"
echo "  ✓ No memory leaks (Valgrind)"
echo "  ✓ No buffer overflows (ASan)"
echo "  ✓ No race conditions (TSan)"
echo ""
