# PRODUCTION GATE DEFINITION

**Date**: 2025-12-27T15:53:00+07:00  
**Purpose**: Single source of truth for production readiness

---

## GATE RULE (Boolean, Machine-Readable)

```bash
# Production ready = ALL SYS_* checks in latest evidence pack are PASS
LATEST_RUN=$(ls -t artifacts/router-e2e/ | head -1)
awk -F'\t' '$1 ~ /^SYS_/ && $2 != "PASS" {exit 1}' \
  artifacts/router-e2e/${LATEST_RUN}/checks.tsv

# Exit 0 = PRODUCTION READY
# Exit 1 = NOT READY
```

---

## REQUIRED CHECKS (Minimum Set)

**Infrastructure**:
- `SYS_NATS_UP` - NATS reachable on port 4222
- `SYS_ROUTER_RUNNING` - Router process alive
- `SYS_GATEWAY_SOCKET` - Gateway socket exists

**Transport**:
- `SYS_IPC_PING` - Ping/pong works
- `SYS_HAPPY_PATH` - >= 90% requests succeed with valid JSON

**Robustness** (Phase 2):
- `SYS_ERROR_SEMANTICS` - 4xx/5xx handled correctly
- `SYS_LATE_REPLY` - Late replies don't leak resources
- `SYS_RECONNECT` - NATS/Router restart recovery works

**Performance** (Phase 3):
- `SYS_BACKPRESSURE` - Backpressure handled without deadlock
- `SYS_CONCURRENT_LOAD` - Sustained load under real Router

---

## CURRENT STATUS

```bash
# Check if any evidence exists
if [ -d "artifacts/router-e2e" ] && [ "$(ls -A artifacts/router-e2e 2>/dev/null)" ]; then
  echo "Evidence exists"
  # Check latest run
  LATEST=$(ls -t artifacts/router-e2e/ | head -1)
  echo "Latest: $LATEST"
  
  # Count checks
  TOTAL=$(awk -F'\t' '$1 ~ /^SYS_/ {print}' artifacts/router-e2e/$LATEST/checks.tsv | wc -l)
  PASS=$(awk -F'\t' '$1 ~ /^SYS_/ && $2 == "PASS" {print}' artifacts/router-e2e/$LATEST/checks.tsv | wc -l)
  FAIL=$(awk -F'\t' '$1 ~ /^SYS_/ && $2 == "FAIL" {print}' artifacts/router-e2e/$LATEST/checks.tsv | wc -l)
  
  echo "SYS checks: PASS=$PASS FAIL=$FAIL TOTAL=$TOTAL"
  
  if [ "$FAIL" -eq 0 ] && [ "$TOTAL" -ge 5 ]; then
    echo "GATE: PASS ✅"
  else
    echo "GATE: FAIL ❌"
  fi
else
  echo "NO EVIDENCE - GATE: FAIL ❌"
fi
```

**As of 2025-12-27**: NO EVIDENCE EXISTS

---

## ENFORCEMENT

**CI Job**: `router-e2e-evidence`
- Runs: `tests/run_router_e2e_evidence_pack.sh`
- Publishes: artifact to persistent storage
- Fails: if any SYS_* check != PASS

**Pre-merge**: Required for production deployment

**Docs guard**: No document may claim "production ready" without:
```markdown
<!-- Production readiness evidence -->
See: artifacts/router-e2e/<timestamp>/checks.tsv
All SYS_* checks: PASS
```

---

## GRADUATION PATH

**Phase 1** (Current):
- Run evidence pack
- Get first artifact (likely failures)
- Status: "System validation in progress"

**Phase 2** (Week 1-2):
- Fix infrastructure checks (NATS/Router/Socket)
- Fix transport checks (Ping/Happy path)
- Status: "Basic integration validated"

**Phase 3** (Week 3-4):
- Add robustness checks
- Fix error semantics/late replies/reconnect
- Status: "Robustness validated"

**Phase 4** (Production):
- All SYS_* checks PASS
- Evidence artifacts in CI
- Status: "PRODUCTION READY"

---

**Gate**: Boolean (PASS/FAIL)  
**Evidence**: Required (no claims without artifacts)  
**Current**: FAIL (no evidence exists)
