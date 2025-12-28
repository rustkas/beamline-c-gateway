#!/bin/bash
# tests/test_gate_negative.sh
#
# NEGATIVE TEST: Gate must FAIL when benchmark fails
# DoD requirement: "сломай один бенч" → gate гарантированно FAIL

set -e

RESULTS_DIR="results/negative_test_$$"
mkdir -p "$RESULTS_DIR"

echo "========================================"
echo "Negative Gate Test"
echo "========================================"
echo ""
echo "Purpose: Verify gate FAILS when benchmark fails"
echo ""

# Simulate benchmark failure by creating fake exit_codes.tsv with failure
cat > "$RESULTS_DIR/exit_codes.tsv" << 'EOF'
benchmark	exit_code	status
warmup	0	PASS
throughput_64b	0	PASS
throughput_256b	1	FAIL
throughput_1024b	0	PASS
latency_64b	0	PASS
latency_256b	0	PASS
latency_1024b	0	PASS
EOF

# Create minimal summary.json
cat > "$RESULTS_DIR/summary.json" << 'EOF'
{
  "gate_pass": false,
  "timestamp": "2025-12-28T13:00:00+07:00"
}
EOF

echo "Created fake results with ONE FAILURE (throughput_256b)"
echo ""

# Run gate check
echo "=== Running Gate Check ==="
if ./benchmarks/check_bench_gate.sh "$RESULTS_DIR" > /dev/null 2>&1; then
    echo "❌ NEGATIVE TEST FAILED: Gate passed when it should FAIL!"
    echo ""
    echo "This is a critical DoD violation:"
    echo "  Gate must FAIL when any benchmark has non-zero exit code"
    rm -rf "$RESULTS_DIR"
    exit 1
else
    echo "✅ NEGATIVE TEST PASSED: Gate correctly FAILED"
    echo ""
    echo "Gate detected benchmark failure as expected"
    rm -rf "$RESULTS_DIR"
    exit 0
fi
