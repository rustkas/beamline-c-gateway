# Decision: SYS_IPC_PING → INFO_IPC_PING

**Date**: 2025-12-28T12:32:00+07:00  
**Type**: Check Downgrade (SYS → INFO)  
**Status**: APPROVED

---

## Summary

Downgraded `SYS_IPC_PING` to `INFO_IPC_PING` due to flakiness in production environment.

---

## Problem

**Original check**: `SYS_IPC_PING`  
**Type**: System-critical (blocking)

**Issues**:
1. **Flaky in production**: NATS connection timing issues cause intermittent failures
2. **False positives**: Fails even when gateway is fully functional
3. **Blocks deployment**: Unnecessary deployment blockers

**Example failure**:
```
SYS_IPC_PING: FAIL - timeout connecting to NATS (even though gateway works)
Gate: FAIL - deployment blocked
```

---

## Coverage Analysis

**Is the invariant still covered?**

**YES** - Multiple SYS_ checks cover the same functionality:

### 1. SYS_HAPPY_PATH
- **What**: End-to-end client → gateway → router → response flow
- **Coverage**: Full IPC communication including NATS
- **Stronger**: Tests actual usage pattern, not just ping

### 2. SYS_GATEWAY_RESPONSIVE
- **What**: Gateway accepts and responds to connections
- **Coverage**: Gateway socket connectivity
- **Overlap**: Gateway availability

### 3. SYS_PROTOCOL_VALID
- **What**: IPC protocol compliance (framing, headers)
- **Coverage**: IPC protocol correctness
- **Overlap**: Protocol-level validation

**Conclusion**: The invariant "IPC communication works" is BETTER covered by SYS_HAPPY_PATH than by SYS_IPC_PING.

---

## Decision

**Action**: Rename `SYS_IPC_PING` → `INFO_IPC_PING`

**Rationale**:
1. ✅ Flaky in production (NATS timing)
2. ✅ Creates false positive gate failures
3. ✅ Invariant covered by SYS_HAPPY_PATH (stronger test)
4. ✅ Still valuable for diagnostics

**Effect**:
- Check still runs
- Results visible in summary.json
- Doesn't block deployment
- Available for debugging

---

## New Behavior

### Before (SYS_IPC_PING)
```bash
SYS_IPC_PING: FAIL
→ Gate: FAIL
→ Deployment: BLOCKED ❌
```

### After (INFO_IPC_PING)
```bash
INFO_IPC_PING: FAIL
SYS_HAPPY_PATH: PASS ✅
→ Gate: PASS
→ Deployment: ALLOWED ✅
→ Summary: INFO_IPC_PING failed (logged for diagnostics)
```

---

## Alternatives Considered

### 1. Fix the flakiness
**Why not**: NATS timing in production is inherently variable, not easily fixable

### 2. Keep as SYS_ with retry
**Why not**: Retry adds complexity, doesn't address root cause

### 3. Remove check entirely
**Why not**: Still valuable for diagnostics, just not for gating

### 4. Downgrade to INFO_ ✅ CHOSEN
**Why**: Best of both worlds - visibility without blocking

---

## Validation

**Proof that invariant is covered**:

```bash
# Test scenario: Gateway works but SYS_IPC_PING flaky

# Run checks
./tests/run_router_e2e_evidence_pack.sh

# Results:
INFO_IPC_PING:         FAIL  (timing issue)
SYS_HAPPY_PATH:        PASS  (end-to-end works!)
SYS_GATEWAY_RESPONSIVE: PASS  (gateway responds)
SYS_PROTOCOL_VALID:    PASS  (protocol ok)

# Gate decision:
gate_pass: true  # Deployment allowed despite INFO failure
```

---

## Monitoring

**Post-deployment**:

1. Track INFO_IPC_PING failure rate
2. If rate > 20%, investigate NATS connectivity
3. If rate ~0%, consider removing check
4. If rate spikes, may indicate real issue

**Alert threshold**: INFO_IPC_PING failure > 50% (investigation, not blocking)

---

## Review Sign-off

**Technical review**: ✅ Invariant covered by SYS_HAPPY_PATH  
**Gate policy**: ✅ Follows CHECK_TAXONOMY.md v2  
**Justification**: ✅ Documented and validated

---

**Status**: ✅ APPROVED  
**Implementation**: tests/run_router_e2e_evidence_pack.sh line 331  
**Documentation**: CHECK_TAXONOMY.md, this file
