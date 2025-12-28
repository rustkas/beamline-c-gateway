#!/bin/bash
# soak_with_metrics.sh - Run soak test with RSS/FD monitoring
#
# Purpose: Proper soak test with resource monitoring for staging validation

set -e

DURATION=${1:-1800}  # Default: 30 minutes
THREADS=${2:-8}
SAMPLE_INTERVAL=${3:-5}  # Sample every 5 seconds

ARTIFACT_DIR="artifacts/soak"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_DIR="$ARTIFACT_DIR/$TIMESTAMP"

mkdir -p "$RESULT_DIR"

echo "=========================================="
echo "Soak Test with Resource Monitoring"
echo "=========================================="
echo "Duration:  $DURATION seconds ($(($DURATION/60)) minutes)"
echo "Threads:   $THREADS"
echo "Sampling:  Every $SAMPLE_INTERVAL seconds"
echo "Results:   $RESULT_DIR"
echo ""

# Start soak test
echo "Starting buffer pool soak test..."
cd build || exit 1

./soak-test-buffer-pool $DURATION $THREADS > "../$RESULT_DIR/soak_output.log" 2>&1 &
SOAK_PID=$!

echo "Soak test PID: $SOAK_PID"
cd ..

# Create CSV header
echo "timestamp,elapsed_sec,pid,rss_kb,vsz_kb,fd_count,threads" > "$RESULT_DIR/metrics.csv"

# Monitor loop
echo ""
echo "Monitoring resource usage..."
echo "Time(s) | RSS(KB) | FD | Status"
echo "--------|---------|----|---------"

START_TIME=$(date +%s)

while kill -0 $SOAK_PID 2>/dev/null; do
    # Get current time
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))
    
    # Get process stats
    if ps -p $SOAK_PID > /dev/null 2>&1; then
        # RSS and VSZ
        STATS=$(ps -p $SOAK_PID -o pid=,rss=,vsz= 2>/dev/null)
        read PID RSS VSZ <<< "$STATS"
        
        # FD count
        FD_COUNT=$(ls -1 /proc/$SOAK_PID/fd 2>/dev/null | wc -l)
        
        # Thread count
        THREAD_COUNT=$(ls -1 /proc/$SOAK_PID/task 2>/dev/null | wc -l)
        
        # Save to CSV
        TIMESTAMP_ISO=$(date +%Y-%m-%dT%H:%M:%S)
        echo "$TIMESTAMP_ISO,$ELAPSED,$PID,$RSS,$VSZ,$FD_COUNT,$THREAD_COUNT" >> "$RESULT_DIR/metrics.csv"
        
        # Display (every 30 seconds)
        if [ $((ELAPSED % 30)) -eq 0 ]; then
            printf "%7d | %7d | %2d | Running\n" $ELAPSED $RSS $FD_COUNT
        fi
    fi
    
    sleep $SAMPLE_INTERVAL
done

# Wait for soak test completion
wait $SOAK_PID
EXIT_CODE=$?

FINAL_TIME=$(date +%s)
TOTAL_ELAPSED=$((FINAL_TIME - START_TIME))

echo ""
echo "Soak test completed"
echo "Exit code: $EXIT_CODE"
echo "Total time: $TOTAL_ELAPSED seconds"
echo ""

# Analyze metrics
echo "=== Resource Usage Analysis ==="

# Calculate statistics
python3 - << 'PYTHON_SCRIPT' "$RESULT_DIR/metrics.csv" "$RESULT_DIR/analysis.txt"
import sys
import csv

csv_file = sys.argv[1]
out_file = sys.argv[2]

rss_values = []
fd_values = []

with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        try:
            rss_values.append(int(row['rss_kb']))
            fd_values.append(int(row['fd_count']))
        except (ValueError, KeyError):
            pass

if rss_values:
    rss_min = min(rss_values)
    rss_max = max(rss_values)
    rss_avg = sum(rss_values) / len(rss_values)
    rss_growth = ((rss_max - rss_min) / rss_min * 100) if rss_min > 0 else 0
    
    fd_min = min(fd_values)
    fd_max = max(fd_values)
    fd_avg = sum(fd_values) / len(fd_values)
    
    with open(out_file, 'w') as out:
        out.write(f"RSS Statistics:\n")
        out.write(f"  Min: {rss_min} KB\n")
        out.write(f"  Max: {rss_max} KB\n")
        out.write(f"  Avg: {rss_avg:.0f} KB\n")
        out.write(f"  Growth: {rss_growth:.2f}%\n")
        out.write(f"\n")
        out.write(f"FD Statistics:\n")
        out.write(f"  Min: {fd_min}\n")
        out.write(f"  Max: {fd_max}\n")
        out.write(f"  Avg: {fd_avg:.1f}\n")
        out.write(f"\n")
        out.write(f"Samples: {len(rss_values)}\n")
    
    print("Resource Analysis:")
    print(f"  RSS: {rss_min}-{rss_max} KB (growth: {rss_growth:.2f}%)")
    print(f"  FD:  {fd_min}-{fd_max} (avg: {fd_avg:.1f})")
    print(f"  Samples: {len(rss_values)}")
PYTHON_SCRIPT

# Create summary
cat > "$RESULT_DIR/SUMMARY.md" << EOF
# Soak Test with Metrics - Summary

**Date**: $(date)
**Duration**: $TOTAL_ELAPSED seconds ($(($TOTAL_ELAPSED/60)) minutes)
**Threads**: $THREADS
**Exit Code**: $EXIT_CODE

---

## Test Output

\`\`\`
$(tail -30 "$RESULT_DIR/soak_output.log")
\`\`\`

---

## Resource Usage

$(cat "$RESULT_DIR/analysis.txt" 2>/dev/null || echo "Analysis not available")

---

## Artifacts

- Full output: \`soak_output.log\`
- Metrics CSV: \`metrics.csv\`
- Analysis: \`analysis.txt\`

---

## Verdict

$(if [ $EXIT_CODE -eq 0 ]; then echo "✅ PASS - Soak test completed successfully"; else echo "❌ FAIL - Soak test failed"; fi)

EOF

echo ""
echo "=========================================="
echo "Soak Test Complete"
echo "=========================================="
echo ""
echo "Results: $RESULT_DIR/"
echo "Summary: $RESULT_DIR/SUMMARY.md"
echo "Metrics: $RESULT_DIR/metrics.csv"
echo ""

if [ -f "$RESULT_DIR/analysis.txt" ]; then
    cat "$RESULT_DIR/analysis.txt"
fi
