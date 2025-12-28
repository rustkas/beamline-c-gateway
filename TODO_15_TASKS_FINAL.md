# 15 TASKS - EXECUTION PLAN

**Date**: 2025-12-27T15:35:00+07:00  
**Source**: User's specifications  
**Status**: READY TO EXECUTE

---

## PHASE 1: BENCHMARKS (Tasks 1-6)

### Task 1: bench_ipc_latency.c
- [ ] Apply unified diff
- [ ] Build with ipc_protocol
- [ ] Test: `./bench-ipc-latency 1000 -p 256`
- [ ] Verify: real framing, canonical socket

### Task 2: bench_ipc_throughput.c
- [ ] Apply unified diff
- [ ] Test: `./bench-ipc-throughput -d 10 -p 256`
- [ ] Verify: RPS output

### Task 3: load_test.sh
- [ ] Apply unified diff
- [ ] Test run
- [ ] Verify: timestamped results/

### Task 4: run_benchmarks.sh
- [ ] Apply unified diff
- [ ] Verify: socket check (not pgrep)
- [ ] Verify: timestamped output

### Task 5: BENCHMARK_PLAN.md
- [ ] Apply unified diff
- [ ] Review: contract section
- [ ] Remove: CPU/flamegraph refs

### Task 6: Build integration
- [ ] Update CMakeLists.txt (if needed)
- [ ] Verify: bench targets build
- [ ] Add to CI (optional)

---

## PHASE 2: EVIDENCE PACK (Tasks 7-9)

### Task 7: Evidence pack DoD
- [ ] Define required E2E checks
- [ ] Document checks.tsv format
- [ ] Define gate rules

### Task 8: Evidence pack generator
- [ ] Save run_router_e2e_evidence_pack.sh
- [ ] Make executable
- [ ] Document ENV requirements

### Task 9: Readiness scorer
- [ ] Create calculate_readiness.sh
- [ ] Input: checks.tsv
- [ ] Output: JSON score

---

## PHASE 3: ROUTER E2E (Tasks 10-15)

### Task 10: Router start command
- [ ] Choose: release vs rebar3 shell
- [ ] Document in .env.example
- [ ] Test startup

### Task 11: Client runner
- [ ] Verify: tests/test_ipc_client.py
- [ ] Or: build minimal C client
- [ ] Test: happy path request

### Task 12: E2E Happy path
- [ ] Script: e2e_happy_path.sh
- [ ] 1000 requests, p95/p99
- [ ] Exit 0 on pass

### Task 13: E2E Error semantics
- [ ] Script: e2e_errors.sh
- [ ] Test: 4xx/5xx mapping
- [ ] Exit 0 on pass

### Task 14: E2E Timeouts/late
- [ ] Script: e2e_timeouts.sh
- [ ] Verify: no leaks
- [ ] Exit 0 on pass

### Task 15: E2E Reconnect storm
- [ ] Script: e2e_reconnect.sh
- [ ] NATS restart + in-flight
- [ ] Exit 0 on pass

---

## DEPENDENCIES

**Need from User**:
1. Router start command (exact)
2. Gateway start command (verified: `./build/c-gateway`)
3. Client tool (verified: `tests/test_ipc_client.py`)

---

**Total**: 15 tasks  
**Phases**: 3  
**Blocked on**: Router start command
