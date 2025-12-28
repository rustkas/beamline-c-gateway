#!/bin/bash
# benchmarks/check_bench_gate.sh
#
# B9: One-command bench suite gate
# Reads ONLY artifacts (exit_codes.tsv, summary.json)
# Returns 0 (PASS) or 1 (FAIL)

set -e

RESULTS_DIR="${1:-results}"

echo "========================================"
echo "Benchmark Suite Gate Check"
echo "========================================"
echo ""

# Find latest results
if [ ! -d "$RESULTS_DIR" ]; then
    echo "❌ FAIL: No results directory found at $RESULTS_DIR"
    echo "Run: ./benchmarks/run_benchmarks.sh"
    exit 1
fi

LATEST_RUN=$(ls -t "$RESULTS_DIR" 2>/dev/null | grep -E '^[0-9]{8}_[0-9]{6}$' | head -1)
if [ -z "$LATEST_RUN" ]; then
    echo "❌ FAIL: No timestamped result directories found"
    echo "Run: ./benchmarks/run_benchmarks.sh"
    exit 1
fi

RESULT_PATH="$RESULTS_DIR/$LATEST_RUN"
EXIT_CODES_FILE="$RESULT_PATH/exit_codes.tsv"
SUMMARY_JSON="$RESULT_PATH/summary.json"

echo "Latest run: $LATEST_RUN"
echo "Results: $RESULT_PATH"
echo ""

# Check files exist
if [ ! -f "$EXIT_CODES_FILE" ]; then
    echo "❌ FAIL: exit_codes.tsv not found"
    exit 1
fi

if [ ! -f "$SUMMARY_JSON" ]; then
    echo "❌ FAIL: summary.json not found"
    exit 1
fi

echo "✓ Artifacts found"
echo ""

# Parse exit_codes.tsv
echo "=== Checking Exit Codes ==="
FAIL_COUNT=0
PASS_COUNT=0

while IFS=$'\t' read -r benchmark exit_code status; do
    # Skip header
    if [ "$benchmark" = "benchmark" ]; then
        continue
    fi
    
    if [ "$status" = "PASS" ]; then
        echo "✅ PASS: $benchmark (exit $exit_code)"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        echo "❌ FAIL: $benchmark (exit $exit_code)"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done < "$EXIT_CODES_FILE"

echo ""
echo "=== Results ==="
echo "PASS: $PASS_COUNT"
echo "FAIL: $FAIL_COUNT"
echo ""

# Check gate_pass in summary.json
if command -v jq > /dev/null 2>&1; then
    GATE_PASS=$(jq -r '.gate_pass' "$SUMMARY_JSON" 2>/dev/null || echo "false")
    echo "summary.json gate_pass: $GATE_PASS"
else
    echo "Warning: jq not found, skipping summary.json check"
    GATE_PASS="unknown"
fi

echo ""

# Determine final gate status
if [ $FAIL_COUNT -eq 0 ] && [ $PASS_COUNT -gt 0 ]; then
    echo "✅ BENCHMARK GATE: PASS"
    echo ""
    echo "All benchmarks passed!"
    echo "Evidence: $RESULT_PATH"
    exit 0
else
    echo "❌ BENCHMARK GATE: FAIL"
    echo ""
    echo "Failed benchmarks: $FAIL_COUNT"
    echo "Fix issues and re-run: ./benchmarks/run_benchmarks.sh"
    exit 1
fi
