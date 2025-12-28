# Check Taxonomy - STRENGTHENED

**Version**: 2  
**Updated**: 2025-12-28T12:31:00+07:00  
**Status**: Formalized with explicit SYS_ list and PERF_ policies

---

## Taxonomy Rules

### SYS_ (System-critical, must-pass)

**Definition**: Checks that MUST pass for production deployment.

**Rules**:
1. ✅ **Explicit list** - SYS_ checks MUST be in versioned REQUIRED_CHECKS list
2. ✅ **No auto-discovery** - "all SYS_*" pattern is FORBIDDEN
3. ✅ **Versioned** - Changes to SYS_ list require review + justification
4. ✅ **Blocking** - ANY SYS_ failure blocks deployment

**Current SYS_ checks** (v2):
```bash
# Defined in .gitlab-ci/check-production-readiness.sh
REQUIRED_CHECKS=(
    "SYS_NATS_UP"           # NATS connectivity
    "SYS_GATEWAY_RESPONSIVE" # Gateway accepts connections
    "SYS_HAPPY_PATH"        # End-to-end flow works
    "SYS_PROTOCOL_VALID"    # IPC protocol compliance
)
```

**To add SYS_ check**:
- Document WHY it's system-critical
- Get review approval
- Update REQUIRED_CHECKS array
- Bump taxonomy version

---

### INFO_ (Informational, non-blocking)

**Definition**: Checks that provide information but don't block deployment.

**Rules**:
1. ✅ **Visible in summary** - MUST appear in summary.json
2. ✅ **Tracked** - Failures recorded in failed_checks array
3. ❌ **Non-blocking** - Never blocks deployment
4. ✅ **Documented** - Must explain why non-blocking

**Current INFO_ checks**:
```bash
INFO_IPC_PING          # IPC ping test (flaky in production)
INFO_CLIENT_METRICS    # Client metrics collection
```

**Use cases**:
- Tests that are flaky in production environment
- Diagnostic/debug information
- Nice-to-have validations

**Visibility**: Always in summary.json, never in REQUIRED_CHECKS

---

### PERF_ (Performance, threshold-based)

**Definition**: Performance checks with defined thresholds.

**Rules**:
1. ✅ **Must have threshold** - Explicit pass/warn/fail thresholds required
2. ✅ **Policy documented** - What happens on warn vs fail
3. ✅ **Visible in summary** - Shown in summary.json with actual values
4. ⚠️ **Warn by default** - PERF_ failures warn but don't block (configurable)

**Threshold format**:
```bash
PERF_LATENCY_P95: value=8500µs, threshold=10000µs, status=PASS
PERF_THROUGHPUT: value=5200rps, threshold=1000rps, status=PASS
```

**Policy** (default):
- `value < threshold`: PASS (green)
- `value >= threshold * 0.9`: WARN (yellow, logged)
- `value >= threshold`: FAIL (red, logged, optional block)

**Enforcement**:
```yaml
# .gitlab-ci.yml
PERF_BLOCK_ON_FAIL: false  # Default: warn only
PERF_BLOCK_ON_FAIL: true   # Opt-in: block deployment
```

---

## Gate Logic

### Production Readiness Gate

```bash
# .gitlab-ci/check-production-readiness.sh
REQUIRED_CHECKS=(
    # Explicit list - NO wildcards!
    "SYS_NATS_UP"
    "SYS_GATEWAY_RESPONSIVE"
    "SYS_HAPPY_PATH"
    "SYS_PROTOCOL_VALID"
)

# Check each explicitly
for check in "${REQUIRED_CHECKS[@]}"; do
    status=$(grep "^$check" checks.tsv | cut -f2)
    if [ "$status" != "PASS" ]; then
        GATE_FAIL=true
    fi
done
```

**NO pattern matching** ("all SYS_*") - explicit list only!

---

## Summary.json Structure

```json
{
  "gate_pass": false,
  "checks": {
    "total": 8,
    "pass": 6,
    "fail": 2
  },
  "sys_checks": {
    "required": ["SYS_NATS_UP", "SYS_GATEWAY_RESPONSIVE", ...],
    "pass": 3,
    "fail": 1
  },
  "info_checks": [
    {"check": "INFO_IPC_PING", "status": "FAIL", "reason": "flaky_in_prod"}
  ],
  "perf_checks": [
    {
      "check": "PERF_LATENCY_P95",
      "value": 8500,
      "threshold": 10000,
      "status": "PASS"
    }
  ],
  "failed_checks": [
    {"check": "SYS_NATS_UP", "status": "FAIL", "blocking": true}
  ]
}
```

---

## Downgrading SYS_ to INFO_

**Requirements**:

1. **Proof of invalidity** OR **Proof of coverage**:
   - Document why check is flaky/invalid in production
   - OR show another SYS_ check covers the same invariant

2. **Review approval**: Required

3. **Document justification**:
   ```markdown
   ## SYS_IPC_PING → INFO_IPC_PING
   
   **Reason**: Flaky in production (NATS connection timing issues)
   **Coverage**: SYS_HAPPY_PATH validates end-to-end IPC flow
   **Decision**: Downgrade to INFO_ for visibility without blocking
   ```

---

## Example: SYS_IPC_PING Downgrade

### Justification

**Original**: `SYS_IPC_PING` - Direct IPC ping test

**Problem**: 
- Flaky in production environment (NATS timing issues)
- Can fail even when gateway is functional
- Causes false positive gate failures

**Coverage**:
- `SYS_HAPPY_PATH` covers end-to-end IPC flow
- `SYS_GATEWAY_RESPONSIVE` covers gateway connectivity
- `SYS_PROTOCOL_VALID` covers IPC protocol compliance

**Decision**: Rename to `INFO_IPC_PING`
- Still runs and reports in summary.json
- Doesn't block deployment
- Visible for diagnostics

---

## Anti-Patterns

### ❌ FORBIDDEN

1. **Pattern matching SYS_ checks**:
   ```bash
   # WRONG!
   grep "^SYS_" checks.tsv | while read check; do
       # Auto-discovery allows stealth checks
   ```

2. **PERF_ without thresholds**:
   ```bash
   # WRONG!
   PERF_LATENCY: status=WARN  # No threshold!
   ```

3. **Undocumented downgrades**:
   ```bash
   # WRONG!
   # Just renamed SYS_ to INFO_ without justification
   ```

### ✅ CORRECT

1. **Explicit check list**:
   ```bash
   REQUIRED_CHECKS=(
       "SYS_NATS_UP"
       "SYS_GATEWAY_RESPONSIVE"
   )
   ```

2. **PERF_ with thresholds**:
   ```bash
   PERF_LATENCY_P95: value=8500µs, threshold=10000µs, status=PASS
   ```

3. **Documented changes**:
   ```markdown
   See: docs/decisions/CHECK_001_IPC_PING_DOWNGRADE.md
   ```

---

## Versioning

**Current version**: 2

**Changelog**:
- v1: Initial taxonomy (SYS_/INFO_/PERF_)
- v2: Explicit REQUIRED_CHECKS list, PERF_ thresholds, downgrade policy

**Future changes**: Require version bump + changelog entry

---

**Status**: ✅ FORMALIZED  
**Laxehole prevention**: Explicit lists, thresholds, justifications  
**Transparency**: All checks visible in summary.json
