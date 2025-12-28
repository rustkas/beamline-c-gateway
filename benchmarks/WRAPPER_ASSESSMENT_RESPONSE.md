# КРИТИЧЕСКАЯ ОЦЕНКА ОБВЯЗКИ - RESPONSE

**Date**: 2025-12-28T07:42:00+07:00  
**User Assessment**: ✅ 100% CORRECT - Found critical mismatch!

---

## PROBLEM 1: run_benchmarks.sh ↔ throughput mismatch

**User says**: "Скрипт передаёт -p $size, но bench-ipc-throughput не поддерживает -p"

**Reality**: ✅ **АБСОЛЮТНО ВЕРНО!**

**Evidence**:
```bash
# run_benchmarks.sh line 87:
-p $size \    # ❌ THROUGHPUT DOESN'T SUPPORT THIS!

# bench_ipc_throughput.c: NO -p flag in arg parsing!
# Only has: -d, -t, -s, -h
```

**Result**: Script WILL FAIL or silently ignore -p!

**My fault**: I added `-p` to wrapper без adding to binary!

---

## PROBLEM 2: load_test.sh прокидывание -s

**User says**: "не прокидывает -s корректно везде"

**Reality**: ⚠️ **PARTIALLY CORRECT**

**Checking load_test.sh**:
- Line 35: ✅ Has `-s "$IPC_SOCKET_PATH"`
- Line 39: ✅ Has `-s "$IPC_SOCKET_PATH"`
- Line 55: ✅ Has `-s "$IPC_SOCKET_PATH"`
- Line 61: ✅ Has `-s "$IPC_SOCKET_PATH"`

**Actually**: I DID fix this in Step 1527! It's correct now.

**But user concern valid**: Need to verify all invocations

---

## PROBLEM 3: load_test.sh artifacts

**User says**: "Нет timestamped artifacts/директории"

**Reality**: ✅ **CORRECT**

**Evidence**:
```bash
# Line 55, 61:
| tee results/sustained_load.txt
| tee results/spike_load.txt

# ❌ No timestamp!
# ❌ Overwrites previous runs!
# ❌ No meta.env/meta.git
# ❌ No command.txt
```

**This breaks evidence trail!**

---

## PROBLEM 4: load_test.sh warmup

**User says**: "Нет явной warmup-фазы"

**Reality**: ✅ **CORRECT**

Load test jumps straight to stress без warmup.

---

## FIXES NEEDED

### Fix 1: Add -p to bench_ipc_throughput.c (P0)

**Add to arg parsing**:
```c
static size_t g_payload_size = 2;  // Default

// In main():
else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
    g_payload_size = (size_t)atoi(argv[i + 1]);
    i++;
}

// In send_ipc_request():
char *payload = malloc(g_payload_size);
memset(payload, 'A', g_payload_size);
ipc_message_t msg = {
    .type = IPC_MSG_PING,
    .payload = payload,
    .payload_len = g_payload_size
};
```

### Fix 2: load_test.sh timestamps (P1)

```bash
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="results/load_test_$TIMESTAMP"
mkdir -p "$RESULTS_DIR"

# Save meta
echo "duration=$DURATION" > "$RESULTS_DIR/meta.env"
echo "concurrency=$CONCURRENCY" >> "$RESULTS_DIR/meta.env"
git rev-parse HEAD > "$RESULTS_DIR/meta.git" || true

# Use timestamped paths
... | tee "$RESULTS_DIR/sustained_load.txt"
... | tee "$RESULTS_DIR/spike_load.txt"
```

### Fix 3: load_test.sh warmup (P2)

```bash
echo "=== Warmup Phase ==="
./build/bench-ipc-latency -n 100 -s "$IPC_SOCKET_PATH" > /dev/null 2>&1
echo "✓ Warmup complete"
```

---

## USER ACCURACY

**run_benchmarks.sh**: ✅ 100% correct (-p мismatch)  
**load_test.sh -s**: ⚠️ Actually fixed, but good to verify  
**load_test.sh artifacts**: ✅ 100% correct (no timestamps)  
**load_test.sh warmup**: ✅ 100% correct (missing)

**Overall**: ✅ EXCELLENT CATCH!

---

**Status**: Critical mismatch found  
**Action**: Fix throughput -p flag + load_test artifacts
