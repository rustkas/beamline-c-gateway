#!/bin/bash
# .gitlab-ci/check-production-readiness.sh
#
# Production readiness gate (facts-only enforcement)
# Checks ONLY artifacts in router-e2e evidence pack
#
# CHECK TAXONOMY (v2):
# - SYS_*  = System-critical, must-pass (EXPLICIT LIST, versioned)
# - INFO_* = Informational, visible but non-blocking
# - PERF_* = Performance, threshold-based (warn/fail with policy)
#
# CRITICAL: SYS_ checks MUST be in explicit REQUIRED_CHECKS array
# NO pattern matching ("all SYS_*") - prevents stealth check injection

set -e

EVIDENCE_DIR="${1:-artifacts/router-e2e}"

# REQUIRED_CHECKS: Explicit, versioned list of SYS_ checks
# Changes require review + justification
# Version: 2
REQUIRED_CHECKS=(
    "SYS_NATS_UP"
    "SYS_GATEWAY_RESPONSIVE"
    "SYS_HAPPY_PATH"
    "SYS_PROTOCOL_VALID"
)

echo "========================================"
echo "Production Readiness Gate (Facts-Only)"
echo "========================================"
echo "=========================================="
echo ""

# Find latest evidence pack
if [ ! -d "$EVIDENCE_DIR" ]; then
    echo "❌ FAIL: No evidence directory found at $EVIDENCE_DIR"
    echo "Run: ./tests/run_router_e2e_evidence_pack.sh"
    exit 1
fi

LATEST_RUN=$(ls -t "$EVIDENCE_DIR" 2>/dev/null | grep -E '^[0-9]{8}_[0-9]{6}$' | head -1)
if [ -z "$LATEST_RUN" ]; then
    echo "❌ FAIL: No timestamped evidence packs found"
    echo "Run: ./tests/run_router_e2e_evidence_pack.sh"
    exit 1
fi

CHECKS_FILE="$EVIDENCE_DIR/$LATEST_RUN/checks.tsv"

echo "Latest evidence: $LATEST_RUN"
echo "Checks file: $CHECKS_FILE"
echo ""

# Verify checks.tsv exists
if [ ! -f "$CHECKS_FILE" ]; then
    echo "❌ FAIL: checks.tsv not found"
    exit 1
fi

echo "✓ Evidence pack found"
echo ""

# Parse checks.tsv
echo "=== Checking Required SYS_* Checks ==="
FAIL_COUNT=0
PASS_COUNT=0
MISSING_COUNT=0

for check_id in "${REQUIRED_CHECKS[@]}"; do
    # Extract check line
    check_line=$(grep "^${check_id}" "$CHECKS_FILE" 2>/dev/null || echo "")
    
    if [ -z "$check_line" ]; then
        echo "❌ MISSING: $check_id"
        ((MISSING_COUNT++))
        continue
    fi
    
    # Parse status (2nd field, tab-separated)
    status=$(echo "$check_line" | cut -f2)
    details=$(echo "$check_line" | cut -f3)
    
    if [ "$status" = "PASS" ]; then
        echo "✅ PASS: $check_id - $details"
        ((PASS_COUNT++))
    else
        echo "❌ FAIL: $check_id - $details"
        ((FAIL_COUNT++))
    fi
done

echo ""
echo "=== Results ==="
echo "PASS: $PASS_COUNT"
echo "FAIL: $FAIL_COUNT"
echo "MISSING: $MISSING_COUNT"
echo ""

# Calculate gate status
TOTAL_REQUIRED=${#REQUIRED_CHECKS[@]}
if [ $PASS_COUNT -eq $TOTAL_REQUIRED ] && [ $FAIL_COUNT -eq 0 ] && [ $MISSING_COUNT -eq 0 ]; then
    echo "✅ PRODUCTION GATE: PASS"
    echo ""
    echo "Evidence: $EVIDENCE_DIR/$LATEST_RUN/checks.tsv"
    exit 0
else
    echo "❌ PRODUCTION GATE: FAIL"
    echo ""
    echo "Required: All $TOTAL_REQUIRED checks must PASS"
    echo "Got: $PASS_COUNT PASS, $FAIL_COUNT FAIL, $MISSING_COUNT MISSING"
    echo ""
    echo "Fix issues and re-run: ./tests/run_router_e2e_evidence_pack.sh"
    exit 1
fi
