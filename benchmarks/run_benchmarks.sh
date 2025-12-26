#!/bin/bash
# run_benchmarks.sh - Run all performance benchmarks

set -e

echo "========================================"
echo "IPC Gateway Performance Benchmark Suite"
echo "========================================"
echo ""

# Check if IPC server is running
if ! pgrep -x "ipc-gateway" > /dev/null; then
    echo "⚠️  Warning: IPC Gateway not running"
    echo "Please start the server first:"
    echo "  ./build/ipc-gateway &"
    echo ""
    exit 1
fi

# Create results directory
mkdir -p results

# Run throughput benchmark
echo "Running Throughput Benchmark..."
./build/bench-ipc-throughput -d 10 -t 4 | tee results/throughput.txt
echo ""

# Run latency benchmark
echo "Running Latency Benchmark..."
./build/bench-ipc-latency 10000 | tee results/latency.txt
echo ""

# Memory info
echo "Running Memory Benchmark..."
./build/bench-memory | tee results/memory.txt
echo ""

# Generate summary
echo "Generating Summary Report..."
cat > results/summary.md << EOF
# IPC Gateway Performance Benchmark Results

**Date**: $(date)
**Version**: v1.0 Baseline

## Throughput

\`\`\`
$(cat results/throughput.txt)
\`\`\`

## Latency

\`\`\`
$(cat results/latency.txt)
\`\`\`

## Memory

\`\`\`
$(cat results/memory.txt)
\`\`\`

## Conclusion

- Throughput: $(grep "Throughput:" results/throughput.txt || echo "N/A")
- Latency p99: $(grep "p99:" results/latency.txt || echo "N/A")
- Memory: See profiling tools output

EOF

echo "✅ Benchmarks Complete!"
echo "Results saved to: results/summary.md"
echo ""
cat results/summary.md
