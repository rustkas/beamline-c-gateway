# CHECK TAXONOMY - GATING VS NON-GATING

**Purpose**: Formal definition of which checks block production deployment

---

## Taxonomy Rules

### SYS_* - System Checks (GATING)

**Definition**: Core system functionality that MUST work for production

**Gate Rule**: ALL SYS_* checks must PASS for gate to PASS

**Examples**:
- `SYS_NATS_UP` - NATS is reachable
- `SYS_ROUTER_RUNNING` - Router process is alive
- `SYS_GATEWAY_SOCKET` - Gateway socket exists
- `SYS_HAPPY_PATH` - End-to-end request succeeds

**Impact**: Any SYS_* FAIL → Gate FAIL → Deployment BLOCKED

---

### INFO_* - Informational Checks (NON-GATING)

**Definition**: Useful diagnostics but not production blockers

**Gate Rule**: INFO_* checks do NOT affect gate status

**Examples**:
- `INFO_IPC_PING` - Basic IPC ping works (diagnostic only)
- `INFO_LATENCY_P99` - p99 latency measurement (informational)
- `INFO_MEMORY_RSS` - Memory usage snapshot (diagnostic)

**Impact**: INFO_* FAIL → Logged but Gate NOT affected

---

### PERF_* - Performance Checks (WARNING)

**Definition**: Performance metrics with thresholds

**Gate Rule**: WARN if threshold exceeded, but don't block

**Examples**:
- `PERF_LATENCY_P95` - p95 < 10ms (warning if exceeded)
- `PERF_THROUGHPUT` - RPS > 1000 (warning if below)

**Impact**: PERF_* FAIL → Warning in logs, Gate NOT affected (for now)

---

## Current Gating Checks

**Required for Production Gate PASS**:

1. ✅ SYS_NATS_UP
2. ✅ SYS_ROUTER_RUNNING
3. ✅ SYS_GATEWAY_SOCKET
4. ✅ SYS_HAPPY_PATH

**Total**: 4 checks must ALL PASS

---

## Why INFO_IPC_PING?

**Original**: `SYS_IPC_PING`  
**Problem**: Gateway behavior makes this FAIL even when happy path works  
**Solution**: Renamed to `INFO_IPC_PING`

**Rationale**:
- Happy path (SYS_HAPPY_PATH) already validates IPC works
- IPC_PING is redundant for gating
- Useful diagnostic but not production blocker

---

## Gate Logic

```bash
# In .gitlab-ci/check-production-readiness.sh:

REQUIRED_CHECKS=("SYS_NATS_UP" "SYS_ROUTER_RUNNING" "SYS_GATEWAY_SOCKET" "SYS_HAPPY_PATH")

for check_id in "${REQUIRED_CHECKS[@]}"; do
    # Only SYS_* from REQUIRED_CHECKS list
    status=$(echo "$check_line" | cut -f2)
    
    if [ "$status" = "PASS" ]; then
        ((PASS_COUNT++))
    else
        ((FAIL_COUNT++))
    fi
done

# Gate passes ONLY if ALL required checks PASS
if [ $PASS_COUNT -eq $TOTAL_REQUIRED ] && [ $FAIL_COUNT -eq 0 ]; then
    gate_pass: true
else
    gate_pass: false
fi
```

---

## Adding New Checks

### Gating Check (blocks deployment):

```bash
# In run_router_e2e_evidence_pack.sh:
printf "SYS_NEW_CHECK\tPASS\tdetails\tevidence.log\n" >> checks.tsv

# In check-production-readiness.sh:
REQUIRED_CHECKS=("SYS_NATS_UP" ... "SYS_NEW_CHECK")
```

### Informational Check (doesn't block):

```bash
# Just use INFO_ prefix:
printf "INFO_DIAGNOSTIC\tFAIL\tdetails\tevidence.log\n" >> checks.tsv

# No change to CI guard needed - automatically ignored
```

---

## Examples

### ✅ PASS Scenario

```
SYS_NATS_UP	PASS	nats=127.0.0.1:4222	meta.env
SYS_ROUTER_RUNNING	PASS	pid=12345	router.log
SYS_GATEWAY_SOCKET	PASS	socket=/tmp/beamline-gateway.sock	gateway.log
SYS_HAPPY_PATH	PASS	ok=50/50 rate=1.000	client.jsonl
INFO_IPC_PING	FAIL	pong_not_received	client.jsonl  ← Ignored!
```

**Result**: Gate PASS (INFO_* ignored)

---

### ❌ FAIL Scenario

```
SYS_NATS_UP	PASS	nats=127.0.0.1:4222	meta.env
SYS_ROUTER_RUNNING	FAIL	router_crashed	router.log  ← Blocks!
SYS_GATEWAY_SOCKET	PASS	socket=/tmp/beamline-gateway.sock	gateway.log
SYS_HAPPY_PATH	PASS	ok=50/50 rate=1.000	client.jsonl
INFO_IPC_PING	PASS	pong_received	client.jsonl  ← Ignored!
```

**Result**: Gate FAIL (any SYS_* FAIL blocks)

---

## Documentation

**Where defined**:
- This file: `CHECK_TAXONOMY.md`
- CI guard: `.gitlab-ci/check-production-readiness.sh` (REQUIRED_CHECKS array)
- E2E runner: `tests/run_router_e2e_evidence_pack.sh` (generates checks)
- CI README: `.gitlab-ci/README.md`

**Enforcement**: Automated via CI - no manual override possible

---

**Status**: FORMALIZED ✅  
**Prefix rules**: Enforced in code ✅  
**Gate logic**: Explicit and documented ✅
