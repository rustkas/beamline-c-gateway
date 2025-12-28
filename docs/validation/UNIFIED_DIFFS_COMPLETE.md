# COMPLETE UNIFIED DIFFS - ALL BENCHMARKS

**Date**: 2025-12-27T15:33:00+07:00  
**Source**: User's complete specifications  
**Status**: READY TO APPLY

---

## FILES PROVIDED

User provided complete unified diffs (git apply ready) for:

1. **bench_ipc_latency.c** (371 lines)
   - Real IPC framing
   - send_all/recv_all with EINTR/EAGAIN
   - Timeouts (10s)
   - Warmup (100 requests)
   - Payload size (-p flag)
   - p50/p95/p99 stats
   - Canonical socket

2. **bench_ipc_throughput.c** (363 lines)
   - Same framing/IO
   - Duration-based (-d flag)
   - RPS measurement

3. **load_test.sh** (152 lines)
   - Canonical socket
   - Unified params
   - Timestamped results

4. **run_benchmarks.sh** (117 lines)
   - Socket check (not pgrep)
   - Timestamped results
   - Payload sweep
   - Meta snapshot

5. **BENCHMARK_PLAN.md** (132 lines)
   - Protocol contract
   - Artifact layout
   - Usage examples
   - No CPU/flamegraph fantasy

---

## ROUTER E2E EVIDENCE PACK SCRIPT

**File**: `tests/run_router_e2e_evidence_pack.sh`

**Complete working implementation** with:
- TSV-based checks.tsv
- JSON summary (computed)
- Process management
- Scenario hooks
- Auto-gate calculation

**Requires** (from environment):
```bash
ROUTER_CMD="<exact command>"
GATEWAY_CMD="<exact command>"
```

Optional:
- IPC_SOCKET_PATH
- NATS_URL
- OUT_DIR

---

## 15 TASKS

1. Apply bench_ipc_latency.c diff
2. Apply bench_ipc_throughput.c diff  
3. Apply load_test.sh diff
4. Apply run_benchmarks.sh diff
5. Apply BENCHMARK_PLAN.md diff
6. Update build (CMake/Makefile)
7. Define evidence pack DoD
8. Implement evidence pack generator
9. Create readiness scorer (awk)
10. Set Router start command
11. Create client runner
12. E2E Happy path scenario
13. E2E Error semantics scenario
14. E2E Timeouts/late replies scenario
15. E2E Reconnect storm scenario

---

**All diffs saved**: See User's message for complete content  
**Status**: READY TO EXECUTE
