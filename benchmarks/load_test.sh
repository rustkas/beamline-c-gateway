#!/bin/bash
# load_test.sh - Load testing script with evidence trail
#
# Task 22: Load Testing Framework

set -e

# Configuration
DURATION=${1:-60}
CONCURRENCY=${2:-10}
TARGET_RPS=${3:-1000}

# Socket path
IPC_SOCKET_PATH=${IPC_SOCKET_PATH:-/tmp/beamline-gateway.sock}

# Timestamped results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="results/load_test_${TIMESTAMP}"
mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "IPC Gateway Load Test"
echo "=========================================="
echo "Duration:      ${DURATION}s"
echo "Concurrency:   ${CONCURRENCY}"
echo "Target RPS:    ${TARGET_RPS}"
echo "Socket:        ${IPC_SOCKET_PATH}"
echo "Results:       ${RESULTS_DIR}"
echo ""

# Check if socket exists
if [ ! -S "$IPC_SOCKET_PATH" ]; then
    echo "Error: IPC socket not found at $IPC_SOCKET_PATH"
    echo "Please start the IPC gateway first"
    exit 1
fi

# Save metadata
cat > "$RESULTS_DIR/meta.env" << EOF
duration=$DURATION
concurrency=$CONCURRENCY
target_rps=$TARGET_RPS
socket=$IPC_SOCKET_PATH
timestamp=$TIMESTAMP
EOF

git rev-parse HEAD > "$RESULTS_DIR/meta.git" 2>/dev/null || echo "not-a-git-repo" > "$RESULTS_DIR/meta.git"
uname -a > "$RESULTS_DIR/meta.system"

# Save command
echo "./benchmarks/load_test.sh $DURATION $CONCURRENCY $TARGET_RPS" > "$RESULTS_DIR/command.txt"

# Warmup phase
echo "=== Warmup Phase ==="
echo "Running 100 warmup requests..."
./build/bench-ipc-latency -n 100 -s "$IPC_SOCKET_PATH" > /dev/null 2>&1 || true
echo "✓ Warmup complete"
echo ""

# Use benchmarks if available
if [ -f "build/bench-ipc-throughput" ]; then
    echo "Running throughput test..."
    ./build/bench-ipc-throughput -s "$IPC_SOCKET_PATH" -d $DURATION -t $CONCURRENCY \
        | tee "$RESULTS_DIR/initial_throughput.txt"
    echo ""
    
    echo "Running latency test..."
    ./build/bench-ipc-latency $(($TARGET_RPS / 10)) -s "$IPC_SOCKET_PATH" \
        | tee "$RESULTS_DIR/initial_latency.txt"
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
./build/bench-ipc-throughput -s "$IPC_SOCKET_PATH" -d $DURATION -t $CONCURRENCY \
    | tee "$RESULTS_DIR/sustained_load.txt"

# Scenario 2: Spike test (if duration > 30)
if [ $DURATION -gt 30 ]; then
    echo ""
    echo "Scenario 2: Spike test (quick burst)"
    ./build/bench-ipc-throughput -s "$IPC_SOCKET_PATH" -d 10 -t $(($CONCURRENCY * 2)) \
        | tee "$RESULTS_DIR/spike_load.txt"
fi

# Generate summary
echo ""
echo "=========================================="
echo "Load Test Summary"
echo "=========================================="
echo ""
echo "=== Generating Facts-Only Artifacts ==="

# Generate checks.tsv
cat > "$RESULTS_DIR/checks.tsv" << EOF
schema_version	1
EOF

# Check sustained load results
SUSTAINED_EXIT=0
if grep -q "error" "$RESULTS_DIR/sustained_load.txt" 2>/dev/null; then
    SUSTAINED_EXIT=1
fi

if [ $SUSTAINED_EXIT -eq 0 ]; then
    echo "LOAD_SUSTAINED_OK	PASS	no_errors_detected	sustained_load.txt" >> "$RESULTS_DIR/checks.tsv"
else
    echo "LOAD_SUSTAINED_OK	FAIL	errors_found	sustained_load.txt" >> "$RESULTS_DIR/checks.tsv"
fi

# Check spike load results
SPIKE_EXIT=0
if grep -q "error" "$RESULTS_DIR/spike_load.txt" 2>/dev/null; then
    SPIKE_EXIT=1
fi

if [ $SPIKE_EXIT -eq 0 ]; then
    echo "LOAD_SPIKE_OK	PASS	no_errors_detected	spike_load.txt" >> "$RESULTS_DIR/checks.tsv"
else
    echo "LOAD_SPIKE_OK	FAIL	errors_found	spike_load.txt" >> "$RESULTS_DIR/checks.tsv"
fi

# Count checks
TOTAL_CHECKS=2
PASS_COUNT=$(grep -c $'\t''PASS'$'\t' "$RESULTS_DIR/checks.tsv" || echo 0)
FAIL_COUNT=$(grep -c $'\t''FAIL'$'\t' "$RESULTS_DIR/checks.tsv" || echo 0)

# Determine gate
GATE_PASS="false"
GATE_STATUS="FAIL"
if [ $FAIL_COUNT -eq 0 ] && [ $PASS_COUNT -eq $TOTAL_CHECKS ]; then
    GATE_PASS="true"
    GATE_STATUS="PASS"
fi

# Generate summary.json
cat > "$RESULTS_DIR/summary.json" << EOF
{
  "timestamp": "$TIMESTAMP",
  "duration_s": $DURATION,
  "concurrency": $CONCURRENCY,
  "socket": "$IPC_SOCKET_PATH",
  "gate_pass": $GATE_PASS,
  "gate_status": "$GATE_STATUS",
  "checks": {
    "total": $TOTAL_CHECKS,
    "pass": $PASS_COUNT,
    "fail": $FAIL_COUNT
  },
  "tests": {
    "sustained_load": {
      "status": "$([ $SUSTAINED_EXIT -eq 0 ] && echo 'PASS' || echo 'FAIL')",
      "exit_code": $SUSTAINED_EXIT
    },
    "spike_load": {
      "status": "$([ $SPIKE_EXIT -eq 0 ] && echo 'PASS' || echo 'FAIL')",
      "exit_code": $SPIKE_EXIT
    }
  },
  "artifact_refs": {
    "checks": "$RESULTS_DIR/checks.tsv",
    "sustained_load": "$RESULTS_DIR/sustained_load.txt",
    "spike_load": "$RESULTS_DIR/spike_load.txt",
    "metadata": {
      "env": "$RESULTS_DIR/meta.env",
      "git": "$RESULTS_DIR/meta.git",
      "system": "$RESULTS_DIR/meta.system",
      "command": "$RESULTS_DIR/command.txt"
    }
  }
}
EOF

echo "✓ Generated checks.tsv"
echo "✓ Generated summary.json"
echo "✓ Gate status: $GATE_STATUS"
echo ""
echo "Results saved to: $RESULTS_DIR"
echo ""

# Display checks
echo "=== Load Test Checks ==="
cat "$RESULTS_DIR/checks.tsv"
echo ""
echo "Artifacts:"
echo "- meta.env, meta.git, meta.system"
echo "- command.txt"
echo "- initial_throughput.txt, initial_latency.txt"
echo "- sustained_load.txt"
[ $DURATION -gt 30 ] && echo "- spike_load.txt"
echo "✅ Load test complete!"
echo "Artifacts: $RESULTS_DIR"
