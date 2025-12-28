# 15 TASKS - FINAL IMPLEMENTATION

**Date**: 2025-12-28T10:40:00+07:00  
**Goal**: Complete ALL 15 tasks in full

---

## CURRENT STATUS

**Done**: 5/15 ✅  
**Partial**: 3/15 ⚠️  
**Missing**: 7/15 ❌

---

## REMAINING WORK

### A4. Throughput: help/output contract parity ❌

**Need**:
- Add -h flag showing all options
- Output should print payload size

**Files**: bench_ipc_throughput.c

---

### A5. Negative-path validation ❌

**Need**:
- Add resp_version/resp_type validation in throughput
- Add resp_version/resp_type validation in memory

**Files**: bench_ipc_throughput.c, bench_memory.c

---

### B8. load_test.sh: artifacts as facts-only ❌

**Need**:
- Generate checks.tsv (LOAD_SUSTAINED_OK, LOAD_SPIKE_OK)
- Generate summary.json with gate_pass

**Files**: benchmarks/load_test.sh

---

### B9. One-command bench suite gate ❌

**Need**:
- Create benchmarks/check_bench_gate.sh
- Read exit_codes.tsv + summary.json
- Return 0 (PASS) or 1 (FAIL)

**Files**: benchmarks/check_bench_gate.sh (NEW)

---

### C11. Router E2E: deterministic start/stop ⚠️

**Need**:
- Add explicit "READY" log markers
- Improve readiness wait logic

**Files**: tests/run_router_e2e_evidence_pack.sh

---

### C12. checks.tsv schema versioning ❌

**Need**:
- Add first line: schema_version\t1
- Or create meta.schema file

**Files**: tests/run_router_e2e_evidence_pack.sh

---

### C13. summary.json must include gate inputs ⚠️

**Need**:
- Add failed_checks array
- Add artifact_refs (router.log, gateway.log, etc.)

**Files**: tests/run_router_e2e_evidence_pack.sh

---

### D14. CI job: verify build + benches + e2e ⚠️

**Need**:
- Complete .gitlab-ci.yml with:
  - build job
  - benchmarks job
  - e2e job
  - artifact collection

**Files**: .gitlab-ci.yml

---

### D15. CI job: forbid "percent readiness" ❌

**Need**:
- Create CI job that scans docs/
- Fail if finds "X% ready" NOT computed from artifacts
- Only allow computed percentages

**Files**: .gitlab-ci.yml, scripts/check_percent_readiness.sh (NEW)

---

## IMPLEMENTATION ORDER

1. A4 - Throughput help/output (quick)
2. A5 - Negative-path validation (critical)
3. C12 - Schema versioning (quick)
4. C13 - Summary.json improvements (important)
5. B9 - Bench gate script (important)
6. B8 - load_test.sh artifacts (medium)
7. C11 - READY markers (medium)
8. D14 - Complete CI jobs (important)
9. D15 - Forbid percent readiness (important)

---

**Implementing now...**
