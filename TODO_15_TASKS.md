# 15-TASK ACTION PLAN

**Date**: 2025-12-27T11:45:00+07:00  
**Source**: User's specification  
**Status**: TODO

---

## PHASE 1: BENCHMARKS (Tasks 1-6)

### Task 1: bench_ipc_latency.c
- [ ] Apply User's diff (202 lines)
- [ ] Build with ipc_protocol
- [ ] Test with socket
- [ ] Save results/<ts>/latency_*.txt

### Task 2: bench_ipc_throughput.c
- [ ] Apply User's diff (224 lines)
- [ ] Test -p 64/256/1024
- [ ] Save results/<ts>/throughput_*.txt

### Task 3: load_test.sh
- [ ] Apply diff (unified params)
- [ ] Test -s/-t/-p flags
- [ ] Verify canonical socket

### Task 4: run_benchmarks.sh
- [ ] Remove pgrep check
- [ ] Add socket existence check
- [ ] Add meta.env snapshot
- [ ] Add payload sweep matrix

### Task 5: BENCHMARK_PLAN.md
- [ ] Document socket contract
- [ ] Document protocol framing
- [ ] Document artifacts layout
- [ ] Remove old references

### Task 6: CI smoke test
- [ ] Create benchmark CI job
- [ ] Socket check (skip if missing)
- [ ] Artifact format validation

---

## PHASE 2: EVIDENCE PACK (Tasks 7-9)

### Task 7: Format freeze
- [ ] Lock checks.tsv format (TSV)
- [ ] Lock summary.json schema
- [ ] Lock meta.* formats
- [ ] Document in EVIDENCE_PACK_SPEC.md

### Task 8: Router E2E skeleton
- [ ] Create run_router_e2e_evidence_pack.sh
- [ ] Implement meta collection
- [ ] Implement checks.tsv generation
- [ ] Implement summary.json computation

### Task 9: SYS_* DoD
- [ ] Define 5 mandatory scenarios
- [ ] Create ROUTER_E2E_DOD.md
- [ ] List acceptance criteria per scenario

---

## PHASE 3: ROUTER E2E (Tasks 10-13)

### Task 10: Router start harness
- [ ] Create start_router.sh
- [ ] Wait for subscribe-ready
- [ ] Record PID
- [ ] Collect logs

### Task 11: NATS lifecycle
- [ ] Create nats_control.sh
- [ ] Kill/restart functionality
- [ ] Readiness check
- [ ] Log collection

### Task 12: Late reply test
- [ ] Implement delayed response scenario
- [ ] Check FD count before/after
- [ ] Check RSS before/after
- [ ] Verify no memory leaks

### Task 13: Backpressure test
- [ ] Implement throttle scenario
- [ ] Verify subject mapping
- [ ] Verify status codes
- [ ] Verify gateway behavior

---

## PHASE 4: ENFORCEMENT (Tasks 14-15)

### Task 14: Readiness calculator
- [ ] Create calculate_readiness.sh
- [ ] Read checks.tsv
- [ ] Compute system_gate (boolean)
- [ ] Output JSON only (no prose)

### Task 15: Docs guard
- [ ] Create lint_readiness_claims.sh
- [ ] Find claims without artifact links
- [ ] Add to CI (fail on violations)
- [ ] Clean up existing violations

---

**Total**: 15 tasks  
**Phases**: 4  
**Status**: Ready to execute
