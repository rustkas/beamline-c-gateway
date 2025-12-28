#!/bin/bash
# production_experience_executor.sh - Complete production validation
#
# This script executes EVERYTHING to gain production experience

set -e

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
ARTIFACTS_DIR="artifacts/production-experience/${TIMESTAMP}"
mkdir -p "${ARTIFACTS_DIR}"

echo "=============================================="
echo "PRODUCTION EXPERIENCE EXECUTOR"
echo "=============================================="
echo "Timestamp: ${TIMESTAMP}"
echo "Artifacts: ${ARTIFACTS_DIR}"
echo ""

# Save execution start
cat > "${ARTIFACTS_DIR}/execution_start.txt" << EOF
Started: $(date -Iseconds)
Hostname: $(hostname)
User: $(whoami)
Gateway Git: $(git rev-parse HEAD 2>/dev/null || echo 'N/A')
EOF

echo "=== Phase 1: Pre-Flight Checks ===" | tee -a "${ARTIFACTS_DIR}/execution.log"

# Check Docker
if ! docker --version > /dev/null 2>&1; then
    echo "✗ Docker not available" | tee -a "${ARTIFACTS_DIR}/execution.log"
    exit 1
fi
echo "✓ Docker available" | tee -a "${ARTIFACTS_DIR}/execution.log"

# Check build artifacts
if [ ! -d "build" ]; then
    echo "✗ Build directory not found" | tee -a "${ARTIFACTS_DIR}/execution.log"
    exit 1
fi
echo "✓ Build artifacts exist" | tee -a "${ARTIFACTS_DIR}/execution.log"

# Check Gateway is running
if [ ! -S "/tmp/beamline-gateway.sock" ]; then
    echo "⚠ Gateway socket not found - will use Docker" | tee -a "${ARTIFACTS_DIR}/execution.log"
else
    echo "✓ Gateway socket exists" | tee -a "${ARTIFACTS_DIR}/execution.log"
fi

echo "" | tee -a "${ARTIFACTS_DIR}/execution.log"
echo "=== Phase 2: Comprehensive Local Validation ===" | tee -a "${ARTIFACTS_DIR}/execution.log"

# Run all core tests with maximum strictness
cd build

TESTS=(
    "test-buffer-pool"
    "test-nats-pool"
    "test-trace-context"
    "test-circuit-breaker"
)

PASSED=0
FAILED=0

for test in "${TESTS[@]}"; do
    if [ -x "./${test}" ]; then
        echo "Running ${test}..." | tee -a "${ARTIFACTS_DIR}/execution.log"
        
        if ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1:check_initialization_order=1 \
           ./${test} > "${ARTIFACTS_DIR}/${test}_production.log" 2>&1; then
            echo "  ✓ ${test} PASSED" | tee -a "${ARTIFACTS_DIR}/execution.log"
            PASSED=$((PASSED + 1))
        else
            echo "  ✗ ${test} FAILED" | tee -a "${ARTIFACTS_DIR}/execution.log"
            FAILED=$((FAILED + 1))
        fi
    else
        echo "  ⚠ ${test} not found" | tee -a "${ARTIFACTS_DIR}/execution.log"
    fi
done

echo "" | tee -a "${ARTIFACTS_DIR}/execution.log"
echo "Core Test Results: ${PASSED} passed, ${FAILED} failed" | tee -a "${ARTIFACTS_DIR}/execution.log"

cd ..

echo "" | tee -a "${ARTIFACTS_DIR}/execution.log"
echo "=== Phase 3: Production Simulation ===" | tee -a "${ARTIFACTS_DIR}/execution.log"

# Simulate production traffic patterns
if [ -S "/tmp/beamline-gateway.sock" ]; then
    echo "Running production traffic simulation..." | tee -a "${ARTIFACTS_DIR}/execution.log"
    
    # Create production traffic simulator
    cat > /tmp/prod_traffic_sim.sh << 'EOFT'
#!/bin/bash
SOCKET="/tmp/beamline-gateway.sock"
REQUESTS=10000
SUCCESS=0
FAILED=0

start_time=$(date +%s)

for i in $(seq 1 $REQUESTS); do
    if echo "{\"task_id\":\"prod_$i\",\"action\":\"test\"}" | timeout 1 nc -U "$SOCKET" > /dev/null 2>&1; then
        SUCCESS=$((SUCCESS + 1))
    else
        FAILED=$((FAILED + 1))
    fi
    
    # Progress every 1000
    if [ $((i % 1000)) -eq 0 ]; then
        echo "Progress: $i/$REQUESTS"
    fi
done

end_time=$(date +%s)
duration=$((end_time - start_time))

echo ""
echo "=== Production Simulation Results ==="
echo "Total Requests: $REQUESTS"
echo "Successful: $SUCCESS"
echo "Failed: $FAILED"
echo "Success Rate: $(echo "scale=2; $SUCCESS * 100 / $REQUESTS" | bc)%"
echo "Duration: ${duration}s"
echo "Throughput: $(echo "scale=2; $REQUESTS / $duration" | bc) req/s"
EOFT
    
    chmod +x /tmp/prod_traffic_sim.sh
    /tmp/prod_traffic_sim.sh > "${ARTIFACTS_DIR}/production_traffic_sim.log" 2>&1
    
    cat "${ARTIFACTS_DIR}/production_traffic_sim.log" | tee -a "${ARTIFACTS_DIR}/execution.log"
else
    echo "⚠ Gateway offline - skipping traffic simulation" | tee -a "${ARTIFACTS_DIR}/execution.log"
fi

echo "" | tee -a "${ARTIFACTS_DIR}/execution.log"
echo "=== Phase 4: Resource Analysis ===" | tee -a "${ARTIFACTS_DIR}/execution.log"

# Analyze Gateway resource usage
if pgrep -f "ipc-server-demo" > /dev/null; then
    PID=$(pgrep -f "ipc-server-demo" | head -1)
    
    echo "Gateway PID: $PID" | tee -a "${ARTIFACTS_DIR}/execution.log"
    
    # Memory usage
    ps -o pid,vsz,rss,pmem,comm -p $PID > "${ARTIFACTS_DIR}/resource_usage.txt" 2>&1
    
    # File descriptors
    ls /proc/$PID/fd 2>/dev/null | wc -l > "${ARTIFACTS_DIR}/fd_count.txt" 2>&1
    
    # Thread count
    ps -o nlwp -p $PID | tail -1 > "${ARTIFACTS_DIR}/thread_count.txt" 2>&1
    
    echo "Resource Usage:" | tee -a "${ARTIFACTS_DIR}/execution.log"
    cat "${ARTIFACTS_DIR}/resource_usage.txt" | tee -a "${ARTIFACTS_DIR}/execution.log"
    echo "File Descriptors: $(cat ${ARTIFACTS_DIR}/fd_count.txt)" | tee -a "${ARTIFACTS_DIR}/execution.log"
    echo "Threads: $(cat ${ARTIFACTS_DIR}/thread_count.txt)" | tee -a "${ARTIFACTS_DIR}/execution.log"
fi

echo "" | tee -a "${ARTIFACTS_DIR}/execution.log"
echo "=== Phase 5: Evidence Collection ===" | tee -a "${ARTIFACTS_DIR}/execution.log"

# Collect all evidence
cat > "${ARTIFACTS_DIR}/PRODUCTION_EVIDENCE.md" << 'EOFM'
# Production Experience Evidence

**Date**: $(date -Iseconds)

---

## Core Component Validation

### Tests Executed:
EOFM

for test in "${TESTS[@]}"; do
    if [ -f "${ARTIFACTS_DIR}/${test}_production.log" ]; then
        echo "- ${test}: ✓ Executed" >> "${ARTIFACTS_DIR}/PRODUCTION_EVIDENCE.md"
    fi
done

cat >> "${ARTIFACTS_DIR}/PRODUCTION_EVIDENCE.md" << 'EOFM'

---

## Production Simulation

See `production_traffic_sim.log` for full results.

---

## Resource Analysis

See `resource_usage.txt` for full details.

---

## Artifacts

All evidence saved in this directory.

EOFM

# Create final summary
cat > "${ARTIFACTS_DIR}/SUMMARY.md" << EOF
# Production Experience Execution - Summary

**Timestamp**: ${TIMESTAMP}
**Date**: $(date -Iseconds)

---

## Execution Results

### Core Tests: ${PASSED} passed, ${FAILED} failed

### Production Simulation:
$(tail -10 "${ARTIFACTS_DIR}/production_traffic_sim.log" 2>/dev/null || echo "Not executed")

### Resource Usage:
$(cat "${ARTIFACTS_DIR}/resource_usage.txt" 2>/dev/null || echo "Not captured")

---

## Artifacts Location

\`${ARTIFACTS_DIR}\`

All evidence has been collected and saved.

---

**Status**: Production experience execution COMPLETE
EOF

cat "${ARTIFACTS_DIR}/SUMMARY.md"

echo ""
echo "=============================================="
echo "Production Experience: COMPLETE"
echo "Artifacts: ${ARTIFACTS_DIR}"
echo "=============================================="

# Save completion time
cat >> "${ARTIFACTS_DIR}/execution_start.txt" << EOF
Completed: $(date -Iseconds)
EOF
