#!/bin/bash
# Gateway Observability Performance Benchmarking Script
# Measures observability overhead (logging, PII filtering, JSON serialization) under load

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GATEWAY_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${GATEWAY_DIR}/build"
REPORT_DIR="${GATEWAY_DIR}/reports/benchmark"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_FILE="${REPORT_DIR}/observability_benchmark_${TIMESTAMP}.json"

# Benchmark parameters
ITERATIONS=${BENCHMARK_ITERATIONS:-10000}
WARMUP_ITERATIONS=${BENCHMARK_WARMUP:-1000}
THREADS=${BENCHMARK_THREADS:-1}

cd "${GATEWAY_DIR}"

echo "=== Gateway Observability Performance Benchmark ==="
echo ""

# Create report directory
mkdir -p "${REPORT_DIR}"

# Check if performance test exists
if [ ! -f "${BUILD_DIR}/c-gateway-performance-test" ]; then
    echo "Building performance tests..."
    make test-performance
fi

echo "Benchmark parameters:"
echo "  Iterations: ${ITERATIONS}"
echo "  Warmup: ${WARMUP_ITERATIONS}"
echo "  Threads: ${THREADS}"
echo ""

# Run performance tests
echo "Running performance tests..."
"${BUILD_DIR}/c-gateway-performance-test" > /tmp/gateway_perf_output.txt 2>&1 || true

# Parse performance test output
parse_performance_output() {
    local output_file="$1"
    local results=()
    
    # Extract log generation performance
    local logs_per_sec=$(grep -oP 'Log generation performance: \K[\d.]+' "${output_file}" | head -1 || echo "0")
    
    # Extract PII filtering latency
    local pii_latency_ms=$(grep -oP 'PII filtering performance: \K[\d.]+' "${output_file}" | head -1 || echo "0")
    
    # Extract JSON serialization performance
    local json_per_sec=$(grep -oP 'JSON serialization performance: \K[\d.]+' "${output_file}" | head -1 || echo "0")
    
    echo "${logs_per_sec}|${pii_latency_ms}|${json_per_sec}"
}

# Parse results
if [ -f /tmp/gateway_perf_output.txt ]; then
    IFS='|' read -r logs_per_sec pii_latency_ms json_per_sec <<< "$(parse_performance_output /tmp/gateway_perf_output.txt)"
    
    echo "Performance Results:"
    echo "  Log generation: ${logs_per_sec} logs/second"
    echo "  PII filtering: ${pii_latency_ms} ms per entry"
    echo "  JSON serialization: ${json_per_sec} serializations/second"
    echo ""
    
    # Generate JSON report
    cat > "${REPORT_FILE}" <<EOF
{
  "timestamp": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "component": "gateway",
  "benchmark_type": "observability",
  "parameters": {
    "iterations": ${ITERATIONS},
    "warmup_iterations": ${WARMUP_ITERATIONS},
    "threads": ${THREADS}
  },
  "results": {
    "log_generation": {
      "throughput_logs_per_second": ${logs_per_sec:-0},
      "unit": "logs/second"
    },
    "pii_filtering": {
      "latency_ms_per_entry": ${pii_latency_ms:-0},
      "unit": "milliseconds"
    },
    "json_serialization": {
      "throughput_serializations_per_second": ${json_per_sec:-0},
      "unit": "serializations/second"
    }
  },
  "system": {
    "cpu_cores": $(nproc 2>/dev/null || echo "unknown"),
    "memory_total_mb": $(free -m | awk '/^Mem:/ {print $2}' 2>/dev/null || echo "unknown")
  }
}
EOF
    
    echo "✅ Benchmark report generated: ${REPORT_FILE}"
    echo ""
    echo "Report summary:"
    cat "${REPORT_FILE}" | jq '.' 2>/dev/null || cat "${REPORT_FILE}"
else
    echo "⚠️  Performance test output not found"
    exit 1
fi

echo ""
echo "=== Benchmark Complete ==="

