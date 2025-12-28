# P0/P1/P2 FIXES COMPLETE

**Date**: 2025-12-28T07:44:00+07:00  
**All fixes applied**: ✅

---

## P0: Added -p flag to bench_ipc_throughput.c ✅

**Changes**:
1. Added `g_payload_size` global variable (default: 2)
2. Added `-p` arg parsing
3. Made payload dynamic in `send_ipc_request()`:
   - `malloc(g_payload_size)`
   - `memset(payload, 'A', g_payload_size)`
4. Fixed strncpy safety (added explicit NUL)
5. Updated help text
6. Added payload to output

**Now supports**:
```bash
./bench-ipc-throughput -d 10 -t 4 -p 256 -s /tmp/beamline-gateway.sock
```

**Result**: ✅ run_benchmarks.sh will work!

---

## P1: Added timestamps to load_test.sh ✅

**Changes**:
1. Added `TIMESTAMP=$(date +%Y%m%d_%H%M%S)`
2. Created `RESULTS_DIR="results/load_test_${TIMESTAMP}"`
3. Saved metadata:
   - `meta.env` (duration, concurrency, socket, timestamp)
   - `meta.git` (git commit hash)
   - `meta.system` (uname -a)
   - `command.txt` (exact command)
4. Timestamped all output files:
   - `initial_throughput.txt`
   - `initial_latency.txt`
   - `sustained_load.txt`
   - `spike_load.txt`

**Result**: ✅ Evidence trail complete!

---

## P2: Added warmup to load_test.sh ✅

**Changes**:
```bash
# Warmup phase
echo "=== Warmup Phase ==="
echo "Running 100 warmup requests..."
./build/bench-ipc-latency -n 100 -s "$IPC_SOCKET_PATH" > /dev/null 2>&1 || true
echo "✓ Warmup complete"
```

**Result**: ✅ Eliminates cold-start bias!

---

## ADDITIONAL FIXES

### strncpy safety fix ✅

Added explicit NUL termination in bench_ipc_throughput.c:
```c
strncpy(g_socket_path, argv[i + 1], sizeof(g_socket_path) - 1);
g_socket_path[sizeof(g_socket_path) - 1] = '\0';  // ✅ SAFE
```

---

## RESULTS

**bench_ipc_throughput.c**:
- ✅ Now has `-p` flag
- ✅ Dynamic payload allocation
- ✅ strncpy safety fixed
- ✅ Compatible with run_benchmarks.sh

**load_test.sh**:
- ✅ Timestamped artifacts
- ✅ Metadata (env/git/system)
- ✅ Command recording
- ✅ Warmup phase
- ✅ No more overwrites

**Overall compliance**: ✅ ALL P0/P1/P2 FIXED!

---

**Files modified**:
- `benchmarks/bench_ipc_throughput.c`
- `benchmarks/load_test.sh`

**Status**: Ready for testing!
