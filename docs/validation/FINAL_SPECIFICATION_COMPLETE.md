# FINAL COMPLETE SPECIFICATION

**Date**: 2025-12-27T11:45:00+07:00  
**Source**: User's complete specification with diffs + evidence pack + 15 tasks  
**Status**: READY TO IMPLEMENT

---

## PART A: BENCHMARK DIFFS (Complete)

### 1. bench_ipc_latency.c
- ipc_protocol framing (real or fallback)
- Canonical socket /tmp/beamline-gateway.sock
- IPC_SOCKET_PATH env + -s flag
- send_all/recv_all with EINTR/EAGAIN
- Timeouts (SO_SNDTIMEO/SO_RCVTIMEO)
- Warmup (100 requests)
- Payload size (-p flag)
- **Complete diff provided** (202 lines)

### 2. bench_ipc_throughput.c
- Same protocol/I/O as latency
- Duration-based measurement
- **Complete diff provided** (224 lines)

### 3. load_test.sh
- Canonical socket contract
- Unified parameters
- **Complete diff provided**

### 4. run_benchmarks.sh
- Socket-based gating (not process name)
- Payload sweep (64/256/1024)
- Meta snapshot
- **Diff plan provided**

### 5. BENCHMARK_PLAN.md
- Socket contract documentation
- Protocol framing spec
- Artifacts layout
- **Update plan provided**

---

## PART B: EVIDENCE PACK (TSV-based)

### Structure
```
artifacts/router_e2e/<timestamp>/
  meta.env          # Facts only
  meta.git          # Git state
  meta.versions     # Tool versions
  checks.tsv        # SOURCE OF TRUTH (TSV format)
  summary.json      # COMPUTED from checks.tsv
  logs/             # All process logs
```

### checks.tsv Format
```
CHECK_ID<TAB>STATUS<TAB>DETAIL<TAB>ARTIFACT_PATH
```

Status: PASS | FAIL | SKIP

### summary.json (computed)
```json
{
  "run_id": "202512 27_093948",
  "system_gate": false,
  "pass": 17,
  "fail": 1,
  "skip": 3
}
```

### Production Gate
```
system_gate = true IFF (fail_cnt == 0 AND skip_cnt == 0)
```

**Bash script provided** for evidence pack generation

---

## PART C: CRITICAL ASSESSMENT

### Good
- Claims → evidence discipline
- Router E2E risk identified
- Proper protocol validation planned

### Bad/Dangerous
- **Two realities**: repo vs "attached files"
- Process-based checks (pgrep) instead of socket
- Too many percentage docs without machine-readable gate

### Root Cause
**Trust drift**: Multiple versions, inconsistent documentation

---

## PART D: 15 TASKS (Ordered)

1. **bench_ipc_latency.c canonicalization**
   - Apply diff, build, test, save artifacts

2. **bench_ipc_throughput.c canonicalization**
   - Apply diff, test -p 64/256/1024

3. **load_test.sh contract alignment**
   - Apply diff, add -s/-t/-p flags

4. **run_benchmarks.sh: socket-based gating**
   - Remove pgrep, add socket check, meta.env

5. **BENCHMARK_PLAN.md: single source**
   - Rewrite with socket contract

6. **CI job: benchmarks smoke**
   - Socket check, format validation

7. **Evidence Pack v1: format freeze**
   - Lock checks.tsv + summary.json contract

8. **Router E2E runner skeleton**
   - Create run_router_e2e_evidence_pack.sh

9. **SYS_* DoD list (5 scenarios)**
   - Define mandatory checks

10. **Real Router start harness**
    - Script to start router, wait for ready

11. **NATS lifecycle control**
    - Kill/restart script with logs

12. **Late reply safety test**
    - FD/memory leak verification

13. **Backpressure semantics test**
    - Subject/status mapping verification

14. **Readiness calculator**
    - checks.tsv → production gate (boolean)

15. **Docs guard**
    - Lint for claims without artifacts link

---

## IMPLEMENTATION ORDER

**Phase 1 (Benchmarks)**: Tasks 1-6  
**Phase 2 (Evidence Pack)**: Tasks 7-9  
**Phase 3 (Router E2E)**: Tasks 10-13  
**Phase 4 (Enforcement)**: Tasks 14-15

---

**Complete diffs**: See User's message  
**All scripts**: Provided in message  
**Status**: READY TO EXECUTE
