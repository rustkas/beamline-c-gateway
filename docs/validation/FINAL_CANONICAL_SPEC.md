# Complete Git Diffs - FINAL CANONICAL SPECIFICATION

**Date**: 2025-12-27T10:48:00+07:00  
**Source**: User's complete unified git diffs  
**Status**: CANONICAL - FINAL VERSION

---

## COMPLETE DIFFS PROVIDED

User provided ready-to-apply git diffs for:

1. **bench_ipc_latency.c** - Complete implementation
   - ipc_protocol.h integration
   - send_all/recv_all with EINTR/EAGAIN
   - Timeouts (10s)
   - Warmup (100 requests)
   - Payload sweep {64,256,1024}
   - Canonical socket /tmp/beamline-gateway.sock
   - IPC_SOCKET_PATH env support

2. **bench_ipc_throughput.c** - Complete implementation
   - Same protocol/I/O as latency
   - Duration-based measurement
   - Payload sweep support

3. **bench_memory.c** - Comment update only
   - Canonical socket path in comment

4. **load_test.sh** - Socket path fix
   - /tmp/ipc-gateway.sock → /tmp/beamline-gateway.sock

---

## EVIDENCE PACK SPECIFICATION

User provided complete TSV-based evidence pack:

**Structure**:
```
artifacts/router-e2e/<ts>/
  environment.txt       # Facts: ts, git_rev, uname, etc.
  command.txt           # Commands executed
  exit_code.txt         # Final exit code
  checks.tsv            # id \t weight \t pass(0/1) \t note
  readiness.tsv         # core_pct \t system_pct \t overall_pct
  logs/
    nats.log
    router.log
    gateway.log
    client.log
```

**Scoring**:
- Core: Only CORE_* checks
- System: Only SYS_* checks  
- Overall: Weighted average of ALL checks
- Auto-calculated via awk (no jq/python)

**Production Gate**: All SYS_* checks must pass

---

## FILES

See User's message for:
- Complete git unified diffs (ready to apply)
- Complete bash scripts:
  - run_router_e2e_with_evidence.sh
  - score_readiness_from_artifacts.sh

---

**Status**: FINAL CANONICAL SPECIFICATION  
**Format**: TSV (facts only, no JSON)  
**Calculation**: awk-based (no external deps)  
**Gate**: Boolean (all SYS_* pass)
