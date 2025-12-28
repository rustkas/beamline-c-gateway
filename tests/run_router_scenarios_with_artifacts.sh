#!/bin/bash
# run_router_scenarios_with_artifacts.sh - Run router scenarios with full evidence
#
# Produces comprehensive artifacts for validation

set -e

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
ARTIFACTS_DIR="artifacts/router-tests/${TIMESTAMP}"

mkdir -p "${ARTIFACTS_DIR}"

echo "=========================================="
echo "Router Scenario Tests with Full Artifacts"
echo "=========================================="
echo ""
echo "Timestamp: ${TIMESTAMP}"
echo "Artifacts: ${ARTIFACTS_DIR}"
echo ""

# Save environment
echo "=== Test Environment ===" > "${ARTIFACTS_DIR}/environment.txt"
echo "Date: $(date -Iseconds)" >> "${ARTIFACTS_DIR}/environment.txt"
echo "Hostname: $(hostname)" >> "${ARTIFACTS_DIR}/environment.txt"
echo "Kernel: $(uname -r)" >> "${ARTIFACTS_DIR}/environment.txt"
echo "GCC: $(gcc --version | head -1)" >> "${ARTIFACTS_DIR}/environment.txt"
echo "Python: $(python3 --version)" >> "${ARTIFACTS_DIR}/environment.txt"
echo "Git Commit: $(git rev-parse HEAD 2>/dev/null || echo 'N/A')" >> "${ARTIFACTS_DIR}/environment.txt"
echo "Git Status: $(git status --porcelain 2>/dev/null | wc -l) files changed" >> "${ARTIFACTS_DIR}/environment.txt"

# Save command
echo "=== Test Command ===" > "${ARTIFACTS_DIR}/command.txt"
echo "Script: $0" >> "${ARTIFACTS_DIR}/command.txt"
echo "Working Directory: $(pwd)" >> "${ARTIFACTS_DIR}/command.txt"
echo "Test Type: Mock Router Integration Scenarios" >> "${ARTIFACTS_DIR}/command.txt"
echo "" >> "${ARTIFACTS_DIR}/command.txt"

# Check prerequisites
echo "Checking prerequisites..."

if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 not found" | tee "${ARTIFACTS_DIR}/error.log"
    exit 1
fi

if ! python3 -c "import nats" 2>/dev/null; then
    echo "ERROR: nats-py not installed" | tee "${ARTIFACTS_DIR}/error.log"
    exit 1
fi

if ! command -v nats-server &> /dev/null; then
    echo "ERROR: nats-server not found" | tee "${ARTIFACTS_DIR}/error.log"
    exit 1
fi

echo "✓ Prerequisites OK"

# Start NATS server
echo ""
echo "=== Starting NATS Server ==="
killall -9 nats-server 2>/dev/null || true
sleep 1

nats-server -p 4222 -a 127.0.0.1 > "${ARTIFACTS_DIR}/nats-server.log" 2>&1 &
NATS_PID=$!
echo "NATS_PID=${NATS_PID}" >> "${ARTIFACTS_DIR}/environment.txt"

sleep 2

if ! kill -0 ${NATS_PID} 2>/dev/null; then
    echo "ERROR: NATS server failed to start" | tee "${ARTIFACTS_DIR}/error.log"
    cat "${ARTIFACTS_DIR}/nats-server.log"
    exit 1
fi

echo "✓ NATS server running (PID: ${NATS_PID})"

# Start Mock Router
echo ""
echo "=== Starting Mock Router ==="
python3 tests/mock_router.py > "${ARTIFACTS_DIR}/mock-router.log" 2>&1 &
ROUTER_PID=$!
echo "ROUTER_PID=${ROUTER_PID}" >> "${ARTIFACTS_DIR}/environment.txt"

sleep 2

if ! kill -0 ${ROUTER_PID} 2>/dev/null; then
    echo "ERROR: Mock Router failed to start" | tee "${ARTIFACTS_DIR}/error.log"
    cat "${ARTIFACTS_DIR}/mock-router.log"
    kill ${NATS_PID} 2>/dev/null || true
    exit 1
fi

echo "✓ Mock Router running (PID: ${ROUTER_PID})"

# Run scenario tests
echo ""
echo "=== Running Scenario Tests ==="
echo "Start Time: $(date -Iseconds)" >> "${ARTIFACTS_DIR}/command.txt"

python3 tests/test_router_scenarios.py > "${ARTIFACTS_DIR}/test-output.log" 2>&1
EXIT_CODE=$?

echo "End Time: $(date -Iseconds)" >> "${ARTIFACTS_DIR}/command.txt"
echo "Exit Code: ${EXIT_CODE}" >> "${ARTIFACTS_DIR}/command.txt"
echo "EXIT_CODE=${EXIT_CODE}" >> "${ARTIFACTS_DIR}/environment.txt"

# Save full output
cat "${ARTIFACTS_DIR}/test-output.log"

# Cleanup
echo ""
echo "=== Cleanup ==="
kill ${ROUTER_PID} 2>/dev/null || true
kill ${NATS_PID} 2>/dev/null || true
sleep 1

echo "✓ Cleanup complete"

# Create summary
echo ""
echo "=== Creating Summary ==="

cat > "${ARTIFACTS_DIR}/SUMMARY.md" << EOF
# Router Scenario Tests - Run Summary

**Timestamp**: ${TIMESTAMP}  
**Date**: $(date -Iseconds)

---

## Test Configuration

**Type**: Mock Router Integration Scenarios  
**Components**:
- NATS Server: localhost:4222
- Mock Router: Python (nats-py)
- Test Client: Python

**Environment**:
- Hostname: $(hostname)
- Kernel: $(uname -r)
- GCC: $(gcc --version | head -1)
- Git Commit: $(git rev-parse HEAD 2>/dev/null || echo 'N/A')

---

## Results

**Exit Code**: ${EXIT_CODE}

$(cat "${ARTIFACTS_DIR}/test-output.log" | grep -A 20 "Test Results Summary" || echo "Summary not found in output")

---

## Artifacts

Directory: \`${ARTIFACTS_DIR}\`

Files:
- \`environment.txt\` - Test environment details
- \`command.txt\` - Command and execution info
- \`test-output.log\` - Full test output
- \`nats-server.log\` - NATS server logs
- \`mock-router.log\` - Mock router logs
- \`SUMMARY.md\` - This file

---

## Notes

**Test Type**: Mock Router (NOT real Router)  
**Limitation**: Mock router simulates basic behavior only  
**Coverage**: ~30-40% of real integration scenarios

**For Production**: Requires REAL Router E2E testing

---

**Status**: $(if [ ${EXIT_CODE} -eq 0 ]; then echo "PASSED"; else echo "FAILED"; fi)
EOF

cat "${ARTIFACTS_DIR}/SUMMARY.md"

echo ""
echo "=========================================="
echo "Artifacts saved to: ${ARTIFACTS_DIR}"
echo "Exit code: ${EXIT_CODE}"
echo "=========================================="

exit ${EXIT_CODE}
