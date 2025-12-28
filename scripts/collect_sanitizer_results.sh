#!/bin/bash
# collect_sanitizer_results.sh - Run all sanitizers and save artifacts
#
# Purpose: Generate "facts only" sanitizer results for staging validation

set -e

ARTIFACT_DIR="artifacts/sanitizers"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_DIR="$ARTIFACT_DIR/$TIMESTAMP"

mkdir -p "$RESULT_DIR"

echo "=========================================="
echo "Sanitizer Validation - Artifact Collection"
echo "=========================================="
echo "Results will be saved to: $RESULT_DIR"
echo ""

cd build || exit 1

# Rebuild without sanitizers first (clean build)
echo "=== Rebuilding for sanitizer tests ==="
cmake .. -DCMAKE_C_FLAGS="-g -gdwarf-4" > /dev/null 2>&1

COMPONENTS=(
    "test-buffer-pool"
    "test-nats-pool"
    "test-trace-context"
    "test-circuit-breaker"
)

## 1. Valgrind
echo "=== Running Valgrind ===" 
for comp in "${COMPONENTS[@]}"; do
    make $comp > /dev/null 2>&1
    echo "Valgrind: $comp..."
    valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --verbose \
             --log-file="../$RESULT_DIR/valgrind_${comp}.log" \
             ./$comp > /dev/null 2>&1
    
    # Extract summary
    grep -E "(LEAK SUMMARY|ERROR SUMMARY|in use at exit)" "../$RESULT_DIR/valgrind_${comp}.log" \
        >> "../$RESULT_DIR/valgrind_summary.txt" || true
done

## 2. AddressSanitizer
echo "=== Running AddressSanitizer ==="
cmake .. -DCMAKE_C_FLAGS="-g -fsanitize=address -fno-omit-frame-pointer" \
         -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" > /dev/null 2>&1

for comp in "${COMPONENTS[@]}"; do
    make $comp > /dev/null 2>&1
    echo "ASan: $comp..."
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=0 \
        ./$comp > "../$RESULT_DIR/asan_${comp}.log" 2>&1 || true
    
    # Extract summary
    grep -E "(ERROR:|leaked|SUMMARY)" "../$RESULT_DIR/asan_${comp}.log" \
        >> "../$RESULT_DIR/asan_summary.txt" || true
done

## 3. UndefinedBehaviorSanitizer
echo "=== Running UBSan ==="
cmake .. -DCMAKE_C_FLAGS="-g -fsanitize=undefined" \
         -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" > /dev/null 2>&1

for comp in "${COMPONENTS[@]}"; do
    make $comp > /dev/null 2>&1
    echo "UBSan: $comp..."
    ./$comp > "../$RESULT_DIR/ubsan_${comp}.log" 2>&1 || true
    
    # Extract summary
    grep -E "(runtime error|SUMMARY)" "../$RESULT_DIR/ubsan_${comp}.log" \
        >> "../$RESULT_DIR/ubsan_summary.txt" || true
done

## 4. ThreadSanitizer (on thread-safe components only)
echo "=== Running ThreadSanitizer ==="
cmake .. -DCMAKE_C_FLAGS="-g -fsanitize=thread" \
         -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" > /dev/null 2>&1

THREAD_COMPONENTS=("test-buffer-pool" "test-nats-pool")

for comp in "${THREAD_COMPONENTS[@]}"; do
    make $comp > /dev/null 2>&1
    echo "TSan: $comp..."
    ./$comp > "../$RESULT_DIR/tsan_${comp}.log" 2>&1 || true
    
    # Extract summary
    grep -E "(WARNING|data race|SUMMARY)" "../$RESULT_DIR/tsan_${comp}.log" \
        >> "../$RESULT_DIR/tsan_summary.txt" || true
done

cd ..

# Create summary report
echo "=== Generating Summary Report ==="

cat > "$RESULT_DIR/SUMMARY.md" << 'EOF'
# Sanitizer Validation Results

**Date**: $(date)
**Components Tested**: 4

---

## Valgrind Results

```
$(cat "$RESULT_DIR/valgrind_summary.txt" 2>/dev/null || echo "No results")
```

---

## AddressSanitizer Results

```
$(cat "$RESULT_DIR/asan_summary.txt" 2>/dev/null || echo "No results")
```

---

## UndefinedBehaviorSanitizer Results

```
$(cat "$RESULT_DIR/ubsan_summary.txt" 2>/dev/null || echo "No results")
```

---

## ThreadSanitizer Results

```
$(cat "$RESULT_DIR/tsan_summary.txt" 2>/dev/null || echo "No results")
```

---

## Artifacts

All detailed logs saved to: `$RESULT_DIR/`

EOF

eval "echo \"$(cat "$RESULT_DIR/SUMMARY.md")\"" > "$RESULT_DIR/SUMMARY.md"

echo ""
echo "=========================================="
echo "Sanitizer Validation Complete"
echo "=========================================="
echo ""
echo "Results saved to: $RESULT_DIR"
echo ""
echo "Summary: $RESULT_DIR/SUMMARY.md"
echo "Logs: $RESULT_DIR/*.log"
echo ""
