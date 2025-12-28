# RUN_BENCHMARKS.SH - ASSESSMENT

**Date**: 2025-12-27T21:26:00+07:00  
**User Analysis**: ✅ ACCURATE

---

## WHAT IS OK ✅

**Socket handling**:
- ✅ `IPC_SOCKET_PATH="/tmp/beamline-gateway.sock"` (line 13)
- ✅ Socket existence check (line 32)
- ✅ Exports to benchmarks via `-s` flag

**Latency sweep**:
- ✅ `PAYLOAD_SIZES="64 256 1024"` (line 21)
- ✅ Loop through sizes (lines 68-78)
- ✅ Each run: `bench-ipc-latency -p $size`

**Results**:
- ✅ Timestamped results directory
- ✅ Summary generation

---

## WHAT IS PROBLEMATIC ⚠️

### Issue 1: Binary Reference Confusion

**Line 36**: Instructs to start:
```bash
./build/ipc-server-demo $IPC_SOCKET_PATH
```

**Lines 83, 85**: Memory profiling mentions:
```bash
valgrind --tool=massif ./build/ipc-server-demo
heaptrack ./build/ipc-server-demo
```

**Problem**:
- ⚠️ Script references `ipc-server-demo` (test/demo server)
- ⚠️ NOT `c-gateway` (production HTTP gateway)
- ⚠️ These are **different binaries** with different purposes

**Context**:
- `ipc-server-demo`: Simple echo server for IPC benchmarking
- `c-gateway`: Production HTTP gateway (different workload)

**User assessment**: ✅ CORRECT - "не тот процесс"

---

### Issue 2: Throughput Benchmark Parameters

**Lines 54-58**: Single throughput run:
```bash
./build/bench-ipc-throughput \
    -d $THROUGHPUT_DURATION \    # 10 seconds
    -t $THROUGHPUT_THREADS \     # 4 threads
    -s "$IPC_SOCKET_PATH"
```

**Missing**:
- ❌ No **warmup** before throughput measurement
- ❌ No **payload size sweep** for throughput (only latency gets sweep)
- ✅ Threads ARE specified (user may have missed this)

**User assessment**: ✅ PARTIALLY CORRECT
- "без warmup" - TRUE ✅
- "без sweep" - TRUE ✅
- "без threads" - FALSE ❌ (threads ARE specified: `-t 4`)

---

## DESIGN QUESTIONS

### Question 1: Which binary to benchmark?

**Options**:
1. **ipc-server-demo**: Simple echo, measures IPC protocol only
2. **c-gateway**: Production HTTP server, different workload
3. **ipc-nats-demo**: IPC with NATS bridge, full E2E

**Current script**: Implies `ipc-server-demo` (lines 36, 83, 85)

**Recommendation**: 
- For IPC protocol benchmarking: `ipc-server-demo` is fine ✅
- For production readiness: Should benchmark actual gateway
- **Document which binary the script expects**

---

### Question 2: Throughput sweep needed?

**Current**:
- Latency: Has payload sweep ✅
- Throughput: Single run, no sweep ❌

**Tradeoff**:
- Pro (current): Faster runs, simpler
- Con (current): Don't know throughput vs payload size

**Recommendation**: Add payload sweep to throughput too

---

## VERDICT

**User's assessment**: ✅ 90% ACCURATE

**Confirmed issues**:
1. ✅ Binary confusion (ipc-server-demo vs c-gateway)
2. ✅ No warmup in throughput
3. ✅ No payload sweep in throughput
4. ❌ Threads ARE specified (user missed `-t 4`)

**Recommendation**: 
- Add comment clarifying which binary to use
- Add warmup to throughput benchmark
- Add payload sweep to throughput (optional but recommended)

---

**Status**: Functional but could be more complete  
**User catch**: Good analysis, minor miss on threads  
**Priority**: Medium (works but not optimal)
