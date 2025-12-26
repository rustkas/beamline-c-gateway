#!/bin/bash
# load_test.sh - Simple load testing script for IPC Gateway
#
# Task 22: Load Testing Framework (simplified version using existing tools)

set -e

# Configuration
DURATION=${1:-60}        # Test duration in seconds (default: 60)
CONCURRENCY=${2:-10}     # Number of concurrent connections (default: 10)
TARGET_RPS=${3:-1000}    # Target requests per second (default: 1000)

# Socket path
SOCKET_PATH=${IPC_SOCKET_PATH:-/tmp/ipc-gateway.sock}

echo "=========================================="
echo "IPC Gateway Load Test"
echo "=========================================="
echo "Duration:      ${DURATION}s"
echo "Concurrency:   ${CONCURRENCY}"
echo "Target RPS:    ${TARGET_RPS}"
echo "Socket:        ${SOCKET_PATH}"
echo ""

# Check if socket exists
if [ ! -S "$SOCKET_PATH" ]; then
    echo "Error: IPC socket not found at $SOCKET_PATH"
    echo "Please start the IPC gateway first"
    exit 1
fi

# Use benchmarks if available
if [ -f "build/bench-ipc-throughput" ]; then
    echo "Running throughput test..."
    ./build/bench-ipc-throughput -d $DURATION -t $CONCURRENCY
    echo ""
    
    echo "Running latency test..."
    ./build/bench-ipc-latency $(($TARGET_RPS / 10))
    echo ""
else
    echo "Warning: Benchmarks not built"
    echo "Run: cd build && make bench-ipc-throughput bench-ipc-latency"
    exit 1
fi

# Load scenarios
echo "=========================================="
echo "Load Scenarios"
echo "=========================================="

#Scenario 1: Sustained load
echo ""
echo "Scenario 1: Sustained load (${DURATION}s)"
./build/bench-ipc-throughput -d $DURATION -t $CONCURRENCY | tee results/sustained_load.txt

# Scenario 2: Spike test (if duration > 30)
if [ $DURATION -gt 30 ]; then
    echo ""
    echo "Scenario 2: Spike test (quick burst)"
    ./build/bench-ipc-throughput -d 10 -t $(($CONCURRENCY * 2)) | tee results/spike_load.txt
fi

# Generate summary
echo ""
echo "=========================================="
echo "Load Test Summary"
echo "=========================================="
echo "Results saved to results/"
echo "- sustained_load.txt"
[ $DURATION -gt 30 ] && echo "- spike_load.txt"
echo ""
echo "✅ Load test complete!"
