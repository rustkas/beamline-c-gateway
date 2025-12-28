# COMPLETE UNIFIED DIFFS - FINAL SPECIFICATION

**Date**: 2025-12-27T11:05:00+07:00  
**Source**: User's complete unified git diffs + Router E2E system  
**Status**: READY TO APPLY

---

## FILES PROVIDED (Complete git diffs)

1. **bench_ipc_latency.c** (446 lines)
   - ipc_protocol.h integration
   - send_all/recv_all with EINTR/EAGAIN
   - Timeouts (10s default)
   - Warmup (100 default)
   - Payload size (-p)
   - p50/p95/p99 percentiles
   - Canonical socket /tmp/beamline-gateway.sock

2. **bench_ipc_throughput.c** (419 lines)
   - Same protocol/I/O
   - RPS measurement
   - Duration-based

3. **bench_memory.c** (352 lines)
   - RSS/FD drift probe
   - CSV output
   - Rate-limited traffic

4. **load_test.sh** (170 lines)
   - Parallel bench_ipc_throughput spawner
   - Timestamped artifacts
   - Facts-only summary

5. **run_benchmarks.sh** (136 lines)
   - Payload sweep runner
   - Timestamped results

6. **BENCHMARK_PLAN.md** (176 lines)
   - Complete build/run docs
   - Facts-only format

---

## ROUTER E2E EVIDENCE PACK (Complete system)

**Structure**:
```
artifacts/router_e2e/<ts>/
  env.json              # Machine-readable environment
  scenarios/*.json      # Per-scenario results
  readiness.json        # AUTO-CALCULATED
  exit_code.txt         # Final verdict
```

**Scripts provided**:
1. `run_router_e2e.sh` - Main runner
2. `run_one_scenario.sh` - Single scenario executor
3. `score_readiness.py` - Auto-scoring

**Production gate**: ALL scenarios pass

---

## KEY FEATURES

**Benchmarks**:
- ✅ ipc_protocol.h (real framing)
- ✅ send_all/recv_all (EINTR/EAGAIN)
- ✅ Timeouts (SO_SNDTIMEO/SO_RCVTIMEO)
- ✅ Canonical socket
- ✅ IPC_SOCKET_PATH env
- ✅ Warmup + payload sweep
- ✅ Zero external deps

**Router E2E**:
- ✅ JSON-based artifacts
- ✅ Auto-calculated readiness
- ✅ Facts only (no prose)
- ✅ Boolean production gate
- ✅ Timestamped evidence

---

## TO APPLY

See User's message for complete diffs (ready to git apply)

Build benchmarks:
```bash
cd benchmarks
mkdir -p build
cc -O2 -Wall -Wextra -Werror -I../include \
   -o build/bench_ipc_latency bench_ipc_latency.c ../src/ipc_protocol.c
cc -O2 -Wall -Wextra -Werror -I../include \
   -o build/bench_ipc_throughput bench_ipc_throughput.c ../src/ipc_protocol.c
cc -O2 -Wall -Wextra -Werror -I../include \
   -o build/bench_memory bench_memory.c ../src/ipc_protocol.c
```

---

**Status**: COMPLETE FINAL SPECIFICATION  
**Total**: 6 diffs + 3 Router E2E scripts  
**Format**: Ready to apply
