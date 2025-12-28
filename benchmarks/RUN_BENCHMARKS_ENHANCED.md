# RUN_BENCHMARKS.SH - ENHANCED

**Date**: 2025-12-27T21:29:00+07:00  
**Changes**: Applied user recommendations

---

## CHANGES MADE

### 1. Binary Documentation ✅

Added comprehensive header:
```bash
# WHICH BINARY TO BENCHMARK:
#   - For IPC protocol benchmarks: ./build/ipc-server-demo (simple echo server)
#   - For production gateway: ./build/c-gateway (HTTP gateway, different workload)
#   - For full E2E: ./build/ipc-nats-demo (IPC + NATS bridge)
#
#   This script benchmarks the IPC PROTOCOL layer specifically.
#   Use ipc-server-demo for consistent IPC-only measurements.
```

**Clarifies**:
- What each binary is for
- Which one this script expects
- Why ipc-server-demo for pure IPC benchmarks

---

### 2. Warmup Phase ✅

Added before benchmarks:
```bash
# Warmup phase
echo "=== Warmup Phase ==="
echo "Running 100 warmup requests..."
./build/bench-ipc-latency -n 100 -s "$IPC_SOCKET_PATH" > /dev/null 2>&1
echo "✓ Warmup complete"
```

**Benefits**:
- Eliminates cold-start bias
- Consistent with latency benchmark internal warmup
- Primes connection pools

---

### 3. Throughput Payload Sweep ✅

Changed from single run to loop:

**Before**:
```bash
./build/bench-ipc-throughput -d 10 -t 4 -s "$SOCKET"
```

**After**:
```bash
for size in $PAYLOAD_SIZES; do  # 64 256 1024
    ./build/bench-ipc-throughput \
        -d $THROUGHPUT_DURATION \
        -t $THROUGHPUT_THREADS \
        -p $size \
        -s "$IPC_SOCKET_PATH"
done
```

**Benefits**:
- Consistent with latency benchmarks
- Measures throughput vs payload size
- More complete performance profile

---

### 4. Summary Updates ✅

Updated summary generation:
- Loops through throughput results for each payload size
- Greps relevant metrics (ops/s, elapsed)
- Separate sections for each size

---

## RESULT

**Now compliant with**:
- ✅ Binary documentation (which to use)
- ✅ Warmup phase (eliminates cold start)
- ✅ Payload sweep for BOTH latency AND throughput
- ✅ Consistent parameter handling

**Output structure**:
```
results/YYYYMMDD_HHMMSS/
  throughput_64b.txt
  throughput_256b.txt
  throughput_1024b.txt
  latency_64b.txt
  latency_256b.txt
  latency_1024b.txt
  summary.md
```

---

**Status**: Enhanced per user recommendations ✅  
**User requests**: All implemented
