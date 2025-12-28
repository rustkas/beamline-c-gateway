# 15 TASKS - NEW BATCH

**Date**: 2025-12-27T15:45:00+07:00  
**Source**: User's Router E2E refinement  
**Focus**: Production-grade E2E validation

---

## PHASE 1: Router Determinism (Tasks 1-4)

### Task 1: Make Router start deterministic
- [ ] Build Router release
- [ ] Use `.../bin/beamline_router foreground`
- [ ] Update ROUTER_CMD default

### Task 2: Router readiness probe
- [ ] Define ready marker (log line)
- [ ] Wait for marker in router.log
- [ ] Timeout = FAIL

### Task 3: Gateway readiness probe
- [ ] Define ready marker
- [ ] Wait for socket + marker
- [ ] Timeout = FAIL

### Task 4: NATS lifecycle
- [ ] Optional: start nats-server in script
- [ ] Or: require external NATS
- [ ] Document in README

---

## PHASE 2: Semantic Testing (Tasks 5-10)

### Task 5: Happy-path strict semantics
- [ ] Require specific JSON fields in response
- [ ] Coordinate with Router team
- [ ] Update check in client.jsonl parser

### Task 6: Headers/trace propagation
- [ ] Send trace_id in request
- [ ] Verify trace_id in response/logs
- [ ] Add SYS_TRACE_PROPAGATION check

### Task 7: Error semantics
- [ ] Test 400/404/500/503
- [ ] Parse Router error responses
- [ ] Add SYS_ERROR_SEMANTICS check

### Task 8: Timeout + late reply
- [ ] Force late reply scenario
- [ ] Check no memory/FD leaks
- [ ] Add SYS_LATE_REPLY check

### Task 9: Reconnect storm
- [ ] Restart NATS/Router 10x
- [ ] Send in-flight requests
- [ ] Add SYS_RECONNECT check

### Task 10: Backpressure
- [ ] Trigger backpressure condition
- [ ] Verify correct status
- [ ] Check no deadlock
- [ ] Add SYS_BACKPRESSURE check

---

## PHASE 3: Infrastructure (Tasks 11-15)

### Task 11: Artifact size discipline
- [ ] Rotate/compress logs
- [ ] Cap artifact size
- [ ] Make CI-friendly

### Task 12: CI job
- [ ] Create router-e2e-evidence job
- [ ] Publish artifacts
- [ ] Fail on any SYS_* FAIL

### Task 13: Readiness scorer
- [ ] Script: calculate_readiness.sh
- [ ] Input: checks.tsv
- [ ] Output: GATE=PASS|FAIL

### Task 14: Docs canonicalization
- [ ] Single page: how to run E2E
- [ ] Link to artifact runs
- [ ] NO percentages

### Task 15: Enforce no claims without artifacts
- [ ] Pre-commit/CI guard
- [ ] Block "PRODUCTION READY" without evidence
- [ ] Require artifact run_id

---

**Total**: 15 tasks  
**Phases**: 3  
**Current blocker**: Router release binary path
