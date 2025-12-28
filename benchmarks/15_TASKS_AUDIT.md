# ПРОВЕРКА 15 ЗАДАЧ - СТАТУС ВЫПОЛНЕНИЯ

**Date**: 2025-12-28T08:20:00+07:00  
**Честная оценка**

---

## Phase A — Бенчи к одному контракту (6 задач)

### ✅ task_bench_memory_make_real - DONE
**Status**: ✅ COMPLETE (Step 1498)

**Evidence**:
- ✅ Использует ipc_protocol framing
- ✅ Canonical socket (-s/ENV/default)
- ✅ send_all/recv_all
- ✅ Timeouts (SO_RCVTIMEO/SO_SNDTIMEO)
- ✅ Exit code != 0 при ошибках

**File**: `benchmarks/bench_memory.c` (228 lines)

---

### ✅ task_bench_throughput_add_payload_flag - DONE
**Status**: ✅ COMPLETE (Step 1583)

**Evidence**:
- ✅ `-p <bytes>` flag added
- ✅ `g_payload_size` variable
- ✅ Dynamic payload allocation: `malloc(g_payload_size)`
- ✅ Actually sends specified size

**Test**: `./bench-ipc-throughput -p 256` sends 256 bytes ✅

---

### ⚠️ task_bench_throughput_add_warmup - PARTIAL
**Status**: ⚠️ INCOMPLETE

**Evidence**:
- ❌ Throughput ITSELF has NO warmup code
- ✅ run_benchmarks.sh has warmup (via latency)
- ❌ No "warmup done" in throughput stdout

**What's missing**: Internal warmup in bench_ipc_throughput.c

**Recommendation**: Add warmup loop to throughput binary OR accept wrapper warmup

---

### ✅ task_bench_throughput_fix_socket_path_copy - DONE
**Status**: ✅ COMPLETE (Step 1583)

**Evidence**:
```c
strncpy(g_socket_path, argv[i + 1], sizeof(g_socket_path) - 1);
g_socket_path[sizeof(g_socket_path) - 1] = '\0';  // ✅ SAFE
```

**DoD**: ✅ No risk of non-null-terminated string

---

### ❌ task_bench_latency_verify_decode_contract - NOT DONE
**Status**: ❌ INCOMPLETE

**What's missing**:
- Need to verify latency checks version/type in response
- Need to verify error handling on bad response
- Need to add explicit validation

**Recommendation**: Review bench_ipc_latency.c decode logic

---

### ⚠️ task_bench_common_socket_contract - MOSTLY DONE
**Status**: ⚠️ PARTIAL

**Evidence**:
- ✅ All use canonical socket `/tmp/beamline-gateway.sock`
- ✅ All support `-s` flag
- ✅ All support `IPC_SOCKET_PATH` env var
- ⚠️ Priority order varies between benchmarks

**What's missing**: Strict consistent priority: CLI > ENV > default

---

## Phase B — Обвязка к детерминированным артефактам (4 задачи)

### ✅ task_run_benchmarks_payload_sweep_consistency - DONE
**Status**: ✅ COMPLETE (Step 1540-1542)

**Evidence**:
```bash
for size in $PAYLOAD_SIZES; do  # 64 256 1024
    ./bench-ipc-throughput -p $size | tee throughput_${size}b.txt
done
```

**DoD**: ✅ results/<ts>/ contains latency_* and throughput_* for all sizes

---

### ⚠️ task_run_benchmarks_add_warmup_gate - MOSTLY DONE
**Status**: ⚠️ PARTIAL

**Evidence**:
- ✅ Warmup phase exists in script
- ✅ Runs before benchmarks
- ❌ Not captured in summary.md

**What's missing**: Warmup cmd + exit code in summary

---

### ✅ task_load_test_socket_propagation - DONE
**Status**: ✅ COMPLETE (Step 1591)

**Evidence**:
```bash
./bench-ipc-throughput -s "$IPC_SOCKET_PATH" ...
./bench-ipc-latency ... -s "$IPC_SOCKET_PATH"
```

**DoD**: ✅ All invocations get socket path

---

### ⚠️ task_benchmark_artifacts_facts_only - PARTIAL
**Status**: ⚠️ INCOMPLETE

**Evidence**:
- ✅ meta.env created
- ✅ meta.git created
- ⚠️ command.txt (not commands.log)
- ❌ exit_codes.tsv - NOT created
- ❌ summary.tsv/json - NOT created (only summary.md)

**What's missing**: Machine-readable summary + exit codes

---

## Phase C — Документация как контракт (2 задачи)

### ✅ task_update_benchmark_plan_to_match_code - DONE
**Status**: ✅ COMPLETE (Step 1551)

**Evidence**: BENCHMARK_PLAN.md includes:
- ✅ Protocol framing (ipc_protocol.h)
- ✅ Canonical socket contract
- ✅ send_all/recv_all requirements
- ✅ Timeouts
- ✅ Warmup phase
- ✅ Payload sweep
- ✅ Artifact structure

**DoD**: ✅ Each point maps to specific file/flag/output

---

### ⚠️ task_document_binary_matrix - PARTIAL
**Status**: ⚠️ INCOMPLETE

**Evidence**:
- ✅ Documented in run_benchmarks.sh header (Step 1540)
- ❌ No docs/operations/benchmarking.md
- ❌ No formal table

**What's missing**: Dedicated documentation file with table

---

## Phase D — Production readiness из артефактов (3 задачи)

### ⚠️ task_router_e2e_evidence_pack_spec - MOSTLY DONE
**Status**: ⚠️ PARTIAL

**Evidence**:
- ✅ checks.tsv format defined
- ✅ meta.env, meta.git, meta.versions
- ✅ router.log, gateway.log
- ✅ client.jsonl
- ❌ summary.json NOT created (only checks.tsv)

**What's missing**: summary.json with gate_pass boolean

---

### ✅ task_router_e2e_runner_real_hooks - MOSTLY DONE
**Status**: ✅ MOSTLY COMPLETE

**Evidence**:
- ✅ Script exists: tests/run_router_e2e_evidence_pack.sh
- ✅ ROUTER_CMD support
- ✅ GATEWAY_CMD support  
- ✅ CLIENT_CMD (inline Python)
- ✅ Parses to checks.tsv
- ⚠️ Exit code logic exists but not explicitly gate-based

**DoD**: ✅ Script runs, generates artifacts, exit code reflects success

---

### ❌ task_ci_guard_no_text_percentages - NOT DONE
**Status**: ❌ NOT IMPLEMENTED

**What's missing**:
- ❌ No CI job defined
- ❌ No enforcement mechanism
- ❌ No guard against markdown percentages

**Recommendation**: Create CI job that parses checks.tsv/summary.json only

---

## SUMMARY

| Phase | Complete | Partial | Missing | Total |
|-------|----------|---------|---------|-------|
| A (Benchmarks) | 3 | 2 | 1 | 6 |
| B (Wrappers) | 2 | 2 | 0 | 4 |
| C (Docs) | 1 | 1 | 0 | 2 |
| D (E2E) | 0 | 2 | 1 | 3 |
| **TOTAL** | **6** | **7** | **2** | **15** |

**Completion**: 40% (6/15) fully done, 87% (13/15) started

---

## CRITICAL GAPS (P0)

1. **task_bench_latency_verify_decode_contract** ❌
   - Need to add response validation

2. **task_ci_guard_no_text_percentages** ❌
   - Need CI enforcement

---

## HIGH PRIORITY (P1)

3. **task_bench_throughput_add_warmup** ⚠️
   - Add internal warmup to throughput

4. **task_benchmark_artifacts_facts_only** ⚠️
   - Add exit_codes.tsv + summary.json

5. **task_router_e2e_evidence_pack_spec** ⚠️
   - Generate summary.json

---

## MEDIUM PRIORITY (P2)

6. **task_bench_common_socket_contract** ⚠️
   - Standardize CLI > ENV > default priority

7. **task_run_benchmarks_add_warmup_gate** ⚠️
   - Capture warmup in summary

8. **task_document_binary_matrix** ⚠️
   - Create formal documentation

---

**Overall Status**: 87% in progress, 40% complete  
**Blocking production**: P0 items (2 tasks)
