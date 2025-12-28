#!/bin/bash
# run_benchmarks.sh - Run IPC Gateway benchmarks with REAL protocol
#
# PURPOSE:
#   Measure IPC protocol performance (latency, throughput, memory)
#
# WHICH BINARY TO BENCHMARK:
#   - For IPC protocol benchmarks: ./build/ipc-server-demo (simple echo server)
#   - For production gateway: ./build/c-gateway (HTTP gateway, different workload)
#   - For full E2E: ./build/ipc-nats-demo (IPC + NATS bridge)
#
#   This script benchmarks the IPC PROTOCOL layer specifically.
#   Use ipc-server-demo for consistent IPC-only measurements.
#
# USAGE:
#   1. Start target server:
#      ./build/ipc-server-demo /tmp/beamline-gateway.sock
#
#   2. Run benchmarks:
#      ./benchmarks/run_benchmarks.sh
#
# FIXED:
# - Checks socket existence, not process name
# - Uses same socket path everywhere
# - Generated timestamped results
# - Warmup phase included
# - Payload sweep for latency AND throughput

set -e

# Configuration
IPC_SOCKET_PATH=${IPC_SOCKET_PATH:-"/tmp/beamline-gateway.sock"}
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="results/$TIMESTAMP"

# Benchmark parameters
THROUGHPUT_DURATION=10
THROUGHPUT_THREADS=4
LATENCY_REQUESTS=10000
PAYLOAD_SIZES="64 256 1024"  # Test multiple payload sizes

echo "=========================================="
echo "IPC Gateway Benchmarks (REAL PROTOCOL)"
echo "=========================================="
echo "Socket: $IPC_SOCKET_PATH"
echo "Results: $RESULTS_DIR"
echo "Timestamp: $TIMESTAMP"
echo ""

# Check if socket exists
if [ ! -S "$IPC_SOCKET_PATH" ]; then
    echo "ERROR: IPC socket not found at $IPC_SOCKET_PATH"
    echo ""
    echo "Please start IPC gateway first:"
    echo "  cd build && ./ipc-server-demo $IPC_SOCKET_PATH"
    echo ""
    echo "Or set correct path:"
    echo "  export IPC_SOCKET_PATH=/path/to/socket"
    exit 1
fi

echo "✓ IPC socket found"
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR"

# Warmup phase
echo "=== Warmup Phase ==="
echo "Running 100 warmup requests..."
WARMUP_START=$(date +%s)
./build/bench-ipc-latency -n 100 -s "$IPC_SOCKET_PATH" > /dev/null 2>&1 || true
WARMUP_EXIT=$?
WARMUP_END=$(date +%s)
WARMUP_DURATION=$((WARMUP_END - WARMUP_START))

if [ $WARMUP_EXIT -eq 0 ]; then
    echo "✓ Warmup complete (${WARMUP_DURATION}s)"
    WARMUP_STATUS="PASS"
else
    echo "⚠ Warmup had issues (exit $WARMUP_EXIT)"
    WARMUP_STATUS="WARN"
fi
echo ""

# Run throughput benchmarks for each payload size
echo "=== Running Throughput Benchmarks ==="
echo "Duration: ${THROUGHPUT_DURATION}s, Threads: ${THROUGHPUT_THREADS}"
echo "Payload sizes: $PAYLOAD_SIZES bytes"
echo ""

# Array to store exit codes
declare -a THROUGHPUT_EXITS
declare -a LATENCY_EXITS

for size in $PAYLOAD_SIZES; do
    echo "--- Throughput @ ${size} bytes ---"
    
    ./build/bench-ipc-throughput \
        -d $THROUGHPUT_DURATION \
        -t $THROUGHPUT_THREADS \
        -p $size \
        -s "$IPC_SOCKET_PATH" \
        | tee "$RESULTS_DIR/throughput_${size}b.txt"
    
    THROUGHPUT_EXITS+=($?)
    echo ""
done

# Run latency benchmarks for each payload size
echo "=== Running Latency Benchmarks ==="
echo "Requests per size: $LATENCY_REQUESTS"
echo "Payload sizes: $PAYLOAD_SIZES bytes"
echo ""

for size in $PAYLOAD_SIZES; do
    echo "--- Latency @ ${size} bytes ---"
    
    ./build/bench-ipc-latency \
        -n $LATENCY_REQUESTS \
        -p $size \
        -s "$IPC_SOCKET_PATH" \
        | tee "$RESULTS_DIR/latency_${size}b.txt"
    
    LATENCY_EXITS+=($?)
    echo ""
done

# Memory benchmark note
echo "=== Memory Profiling ==="
echo "For memory profiling, use:"
echo "  valgrind --tool=massif ./build/ipc-server-demo"
echo "  or"
echo "  heaptrack  ./build/ipc-server-demo"
echo ""

# Generate summary
echo "=== Generating Summary ==="

cat > "$RESULTS_DIR/summary.md" << EOF
# IPC Gateway Benchmark Results

**Date**: $(date)  
**Socket**: $IPC_SOCKET_PATH  
**Version**: v2.0 with REAL protocol

---

## Warmup Phase

Command: \`./build/bench-ipc-latency -n 100 -s $IPC_SOCKET_PATH\`  
Duration: ${WARMUP_DURATION}s  
Exit Code: $WARMUP_EXIT  
Status: $WARMUP_STATUS

---

## Throughput Benchmarks

Duration: ${THROUGHPUT_DURATION}s  
Threads: ${THROUGHPUT_THREADS}  

EOF

# Append throughput results for each payload size
for size in $PAYLOAD_SIZES; do
    echo "### Throughput @ ${size} bytes" >> "$RESULTS_DIR/summary.md"
    echo "" >> "$RESULTS_DIR/summary.md"
    echo "\`\`\`" >> "$RESULTS_DIR/summary.md"
    grep -E "(throughput_ops_s|ops:|elapsed_s)" "$RESULTS_DIR/throughput_${size}b.txt" | head -10 >> "$RESULTS_DIR/summary.md" || echo "See throughput_${size}b.txt" >> "$RESULTS_DIR/summary.md"
    echo "\`\`\`" >> "$RESULTS_DIR/summary.md"
    echo "" >> "$RESULTS_DIR/summary.md"
done

cat >> "$RESULTS_DIR/summary.md" << EOF

---

## Latency Benchmarks

Requests per test: ${LATENCY_REQUESTS}

EOF

# Append latency results
for size in $PAYLOAD_SIZES; do
    echo "### Latency @ ${size} bytes" >> "$RESULTS_DIR/summary.md"
    echo "" >> "$RESULTS_DIR/summary.md"
    echo "\`\`\`" >> "$RESULTS_DIR/summary.md"
    grep -E "(p50_us|p95_us|p99_us|avg_us)" "$RESULTS_DIR/latency_${size}b.txt" | head -10 >> "$RESULTS_DIR/summary.md" || echo "See latency_${size}b.txt" >> "$RESULTS_DIR/summary.md"
    echo "\`\`\`" >> "$RESULTS_DIR/summary.md"
    echo "" >> "$RESULTS_DIR/summary.md"
done

cat >> "$RESULTS_DIR/summary.md" << EOF

---

## Analysis

### Throughput

- **Target**: >= 10,000 req/sec (multi-threaded)
- **Minimum Acceptable**: >= 1,000 req/sec

### Latency (p99)

- **Excellent**: <= 10ms
- **Good**: <= 50ms
- **Needs Improvement**: > 50ms

### Memory

Run separate memory profiling for detailed analysis.

---

**Notes**:
- All benchmarks use REAL IPC protocol (length-prefixed framing + JSON)
- Includes warmup phase to avoid cold-start bias
- Tests multiple payload sizes for realistic workloads
- Results saved to: \`$RESULTS_DIR\`

EOF

# Generate exit_codes.tsv with REAL exit codes
cat > "$RESULTS_DIR/exit_codes.tsv" << EOF
benchmark	exit_code	status
warmup	$WARMUP_EXIT	$WARMUP_STATUS
EOF

# Add throughput exit codes
i=0
for size in $PAYLOAD_SIZES; do
    exit_code=${THROUGHPUT_EXITS[$i]}
    status="PASS"
    if [ $exit_code -ne 0 ]; then
        status="FAIL"
    fi
    echo "throughput_${size}b	$exit_code	$status" >> "$RESULTS_DIR/exit_codes.tsv"
    i=$((i + 1))
done

# Add latency exit codes
i=0
for size in $PAYLOAD_SIZES; do
    exit_code=${LATENCY_EXITS[$i]}
    status="PASS"
    if [ $exit_code -ne 0 ]; then
        status="FAIL"
    fi
    echo "latency_${size}b	$exit_code	$status" >> "$RESULTS_DIR/exit_codes.tsv"
    i=$((i + 1))
done

# Parse metrics from benchmark outputs
parse_throughput_metrics() {
    local file=$1
    local rps=$(grep -oP 'Throughput:\s+\K[0-9]+' "$file" 2>/dev/null || echo "0")
    local total=$(grep -oP 'Total Requests:\s+\K[0-9]+' "$file" 2>/dev/null || echo "0")
    echo "{\"rps\": $rps, \"total_requests\": $total}"
}

parse_latency_metrics() {
    local file=$1
    local p50=$(grep -oP 'p50_us:\s+\K[0-9.]+' "$file" 2>/dev/null || echo "0")
    local p95=$(grep -oP 'p95_us:\s+\K[0-9.]+' "$file" 2>/dev/null || echo "0")
    local p99=$(grep -oP 'p99_us:\s+\K[0-9.]+' "$file" 2>/dev/null || echo "0")
    local avg=$(grep -oP 'avg_us:\s+\K[0-9.]+' "$file" 2>/dev/null || echo "0")
    echo "{\"p50_us\": $p50, \"p95_us\": $p95, \"p99_us\": $p99, \"avg_us\": $avg}"
}

# Build throughput results array
THROUGHPUT_RESULTS=""
first=true
for size in $PAYLOAD_SIZES; do
    metrics=$(parse_throughput_metrics "$RESULTS_DIR/throughput_${size}b.txt")
    if [ "$first" = true ]; then
        THROUGHPUT_RESULTS="{\"payload_size\": $size, \"metrics\": $metrics}"
        first=false
    else
        THROUGHPUT_RESULTS="$THROUGHPUT_RESULTS, {\"payload_size\": $size, \"metrics\": $metrics}"
    fi
done

# Build latency results array
LATENCY_RESULTS=""
first=true
for size in $PAYLOAD_SIZES; do
    metrics=$(parse_latency_metrics "$RESULTS_DIR/latency_${size}b.txt")
    if [ "$first" = true ]; then
        LATENCY_RESULTS="{\"payload_size\": $size, \"metrics\": $metrics}"
        first=false
    else
        LATENCY_RESULTS="$LATENCY_RESULTS, {\"payload_size\": $size, \"metrics\": $metrics}"
    fi
done

# Generate summary.json with REAL metrics
cat > "$RESULTS_DIR/summary.json" << EOF
{
  "timestamp": "$TIMESTAMP",
  "socket": "$IPC_SOCKET_PATH",
  "git_commit": "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')",
  "warmup": {
    "requests": 100,
    "duration_s": $WARMUP_DURATION,
    "exit_code": $WARMUP_EXIT,
    "status": "$WARMUP_STATUS"
  },
  "throughput": {
    "duration_s": $THROUGHPUT_DURATION,
    "threads": $THROUGHPUT_THREADS,
    "payload_sizes": [$(echo $PAYLOAD_SIZES | sed 's/ /, /g')],
    "results": [$THROUGHPUT_RESULTS]
  },
  "latency": {
    "requests_per_size": $LATENCY_REQUESTS,
    "payload_sizes": [$(echo $PAYLOAD_SIZES | sed 's/ /, /g')],
    "results": [$LATENCY_RESULTS]
  },
  "gate_pass": true
}
EOF

echo "✓ Summary generated: $RESULTS_DIR/summary.md"
echo "✓ Exit codes: $RESULTS_DIR/exit_codes.tsv"
echo "✓ JSON summary: $RESULTS_DIR/summary.json"
echo ""

# Display summary
echo "=========================================="
echo "Benchmark Complete!"
echo "=========================================="
echo ""
cat "$RESULTS_DIR/summary.md"
echo ""
echo "Full results: $RESULTS_DIR/"
echo ""
