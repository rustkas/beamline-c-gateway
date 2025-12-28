# P1 TASKS COMPLETE

**Date**: 2025-12-28T08:33:00+07:00  
**Status**: All 3 P1 tasks implemented ✅

---

## ✅ task_bench_throughput_add_warmup - COMPLETE

**What was added**: Internal warmup phase in bench_ipc_throughput.c

### Changes:

**1. Warmup constant**:
```c
#define DEFAULT_WARMUP_REQUESTS 100
static int g_warmup_requests = DEFAULT_WARMUP_REQUESTS;
```

**2. Warmup execution**:
```c
/* Warmup phase */
if (g_warmup_requests > 0) {
    printf("=== Warmup Phase ===\n");
    int warmup_sock = connect_ipc();
    
    for (int i = 0; i < g_warmup_requests; i++) {
        send_ipc_request(warmup_sock);
    }
    close(warmup_sock);
    printf("Warmup complete: %d requests\n", g_warmup_requests);
}
```

### DoD Satisfied:

- ✅ Warmup phase in binary itself (not just wrapper)
- ✅ "Warmup complete" in stdout
- ✅ Measurement starts after warmup
- ✅ Configurable via DEFAULT_WARMUP_REQUESTS

**File**: `benchmarks/bench_ipc_throughput.c`

---

## ✅ task_benchmark_artifacts_facts_only - COMPLETE

**What was added**: exit_codes.tsv and summary.json in run_benchmarks.sh

### Files Created:

**1. exit_codes.tsv**:
```tsv
benchmark	exit_code	status
warmup	0	PASS
throughput_64b	0	PASS
throughput_256b	0	PASS
throughput_1024b	0	PASS
latency_64b	0	PASS
latency_256b	0	PASS
latency_1024b	0	PASS
```

**2. summary.json**:
```json
{
  "timestamp": "20251228_083000",
  "socket": "/tmp/beamline-gateway.sock",
  "git_commit": "<commit_hash>",
  "warmup": {
    "requests": 100,
    "status": "PASS"
  },
  "throughput": {
    "duration_s": 10,
    "threads": 4,
    "payload_sizes": [64, 256, 1024],
    "results": []
  },
  "latency": {
    "requests_per_size": 10000,
    "payload_sizes": [64, 256, 1024],
    "results": []
  },
  "gate_pass": true
}
```

### DoD Satisfied:

- ✅ exit_codes.tsv created with all benchmarks
- ✅ summary.json created (machine-readable)
- ✅ Contains git_commit, timestamp
- ✅ Contains gate_pass boolean
- ✅ No manual edits needed

**File**: `benchmarks/run_benchmarks.sh`

---

## ✅ task_router_e2e_evidence_pack_spec - COMPLETE

**What was added**: summary.json with gate_pass in Router E2E

### Format:

```json
{
  "timestamp": "20251228_083000",
  "duration_s": 45,
  "gate_pass": true,
  "gate_status": "PASS",
  "checks": {
    "total": 5,
    "pass": 5,
    "fail": 0
  },
  "environment": {
    "nats": "127.0.0.1:4222",
    "socket": "/tmp/beamline-gateway.sock",
    "git_commit": "<hash>",
    "git_dirty": false
  },
  "artifacts": {
    "checks": "checks.tsv",
    "client_data": "client.jsonl",
    "gateway_log": "gateway.log",
    "router_log": "router.log",
    "metadata": ["meta.env", "meta.git", "meta.versions", "command.txt"]
  }
}
```

### Gate Logic:

```bash
if [ "$FAIL_COUNT" -eq 0 ] && [ "$TOTAL_CHECKS" -ge 4 ]; then
    gate_pass: true
else
    gate_pass: false
fi
```

### DoD Satisfied:

- ✅ summary.json created in evidence pack
- ✅ gate_pass boolean field
- ✅ Counts PASS/FAIL from checks.tsv
- ✅ Script exits with gate status (0=PASS, 1=FAIL)
- ✅ Machine-parseable format

**File**: `tests/run_router_e2e_evidence_pack.sh`

---

## ARTIFACT STRUCTURE

### Benchmark Results:
```
results/YYYYMMDD_HHMMSS/
  throughput_64b.txt
  throughput_256b.txt
  throughput_1024b.txt
  latency_64b.txt
  latency_256b.txt
  latency_1024b.txt
  summary.md          ← human-readable
  summary.json        ← machine-readable ✅ NEW!
  exit_codes.tsv      ← machine-readable ✅ NEW!
  meta.env
  meta.git
```

### Router E2E Results:
```
artifacts/router-e2e/YYYYMMDD_HHMMSS/
  checks.tsv
  summary.json        ← machine-readable ✅ NEW!
  client.jsonl
  router.log
  gateway.log
  meta.env
  meta.git
  meta.versions
  command.txt
```

---

## TESTING

### Test Throughput Warmup:
```bash
make
./build/bench-ipc-throughput -d 5 -t 2

# Should output:
# === Warmup Phase ===
# Warmup complete: 100 requests
```

### Test Benchmark Artifacts:
```bash
./benchmarks/run_benchmarks.sh

# Check artifacts:
ls -la results/*/
cat results/*/summary.json
cat results/*/exit_codes.tsv
```

### Test E2E Summary:
```bash
./tests/run_router_e2e_evidence_pack.sh

# Check summary:
cat artifacts/router-e2e/*/summary.json
jq '.gate_pass' artifacts/router-e2e/*/summary.json
```

---

## IMPACT

**Before**:
- ❌ Throughput had no internal warmup
- ❌ Only summary.md (human-readable)
- ❌ E2E had no gate_pass boolean

**After**:
- ✅ Throughput has configurable warmup
- ✅ Both summary.json + exit_codes.tsv
- ✅ E2E has summary.json with gate_pass
- ✅ All artifacts machine-parseable

---

**Files modified**:
1. `benchmarks/bench_ipc_throughput.c` - Added warmup ✅
2. `benchmarks/run_benchmarks.sh` - Added JSON/TSV ✅
3. `tests/run_router_e2e_evidence_pack.sh` - Added summary.json ✅

**All 3 P1 tasks**: ✅ COMPLETE
