# CI GUARD DOCUMENTATION

**Purpose**: Enforce production readiness based ONLY on machine-readable artifacts

---

## RULE

**Production readiness is determined by**:
- ✅ `artifacts/router-e2e/<timestamp>/checks.tsv`
- ✅ Exit code from CI guard script

**Production readiness is NOT determined by**:
- ❌ Markdown files with percentages
- ❌ Text descriptions
- ❌ Manual assessments

---

## CI GUARD SCRIPT

**Location**: `.gitlab-ci/check-production-readiness.sh`

**What it does**:
1. Finds latest evidence pack in `artifacts/router-e2e/`
2. Parses `checks.tsv` (tab-separated values)
3. Checks ALL required `SYS_*` checks are PASS
4. Returns exit 0 if PASS, exit 1 if FAIL

**Required checks**:
- `SYS_NATS_UP`
- `SYS_ROUTER_RUNNING`
- `SYS_GATEWAY_SOCKET`
- `SYS_HAPPY_PATH`

---

## GITLAB CI INTEGRATION

**File**: `.gitlab-ci.yml`

**Jobs**:

### 1. `router_e2e_evidence`
- Runs `./tests/run_router_e2e_evidence_pack.sh`
- Generates timestamped evidence pack
- Archives artifacts for 30 days

### 2. `production_readiness_gate`
- Runs CI guard script
- Parses checks.tsv
- **MUST PASS** for deployment
- `allow_failure: false` (blocking)

### 3. `benchmark_regression` (optional)
- Checks p95 latency < 10ms
- Warning only (`allow_failure: true`)

### 4. `deploy_production`
- Only runs if gate passes
- Manual trigger
- Requires production_readiness_gate success

---

## ENFORCEMENT

**Merge Request Pipeline**:
```
test: router_e2e_evidence → artifacts/router-e2e/<ts>/
  ↓
validate: production_readiness_gate → PASS/FAIL
  ↓
(if PASS) deploy: deploy_production (manual)
```

**Main Branch Pipeline**: Same + auto-deploy option

---

## EXAMPLE

### ✅ PASS Scenario

**checks.tsv**:
```
SYS_NATS_UP	PASS	nats=127.0.0.1:4222	meta.env
SYS_ROUTER_RUNNING	PASS	pid=12345	router.log
SYS_GATEWAY_SOCKET	PASS	socket=/tmp/beamline-gateway.sock	gateway.log
SYS_HAPPY_PATH	PASS	ok=50/50 rate=1.000 p95_us=4630	client.jsonl
```

**CI Output**:
```
✅ PASS: SYS_NATS_UP - nats=127.0.0.1:4222
✅ PASS: SYS_ROUTER_RUNNING - pid=12345
✅ PASS: SYS_GATEWAY_SOCKET - socket=/tmp/beamline-gateway.sock
✅ PASS: SYS_HAPPY_PATH - ok=50/50 rate=1.000 p95_us=4630

PASS: 4
FAIL: 0
MISSING: 0

✅ PRODUCTION GATE: PASS
```

**Result**: Deployment allowed

---

### ❌ FAIL Scenario

**checks.tsv**:
```
SYS_NATS_UP	PASS	nats=127.0.0.1:4222	meta.env
SYS_ROUTER_RUNNING	FAIL	router_crashed	router.log
SYS_GATEWAY_SOCKET	PASS	socket=/tmp/beamline-gateway.sock	gateway.log
SYS_HAPPY_PATH	FAIL	ok=25/50 rate=0.500	client.jsonl
```

**CI Output**:
```
✅ PASS: SYS_NATS_UP - nats=127.0.0.1:4222
❌ FAIL: SYS_ROUTER_RUNNING - router_crashed
✅ PASS: SYS_GATEWAY_SOCKET - socket=/tmp/beamline-gateway.sock
❌ FAIL: SYS_HAPPY_PATH - ok=25/50 rate=0.500

PASS: 2
FAIL: 2
MISSING: 0

❌ PRODUCTION GATE: FAIL
```

**Result**: Deployment blocked, pipeline fails

---

## LOCAL TESTING

```bash
# Generate evidence pack
./tests/run_router_e2e_evidence_pack.sh

# Run CI guard locally
./.gitlab-ci/check-production-readiness.sh

# Check exit code
echo $?  # 0 = PASS, 1 = FAIL
```

---

## CRITICAL RULES

1. **NO markdown percentages in gate logic**
   - STATUS.md, HONEST_ASSESSMENT.md are for humans
   - checks.tsv is source of truth

2. **Gate is boolean**
   - PASS = all required checks PASS
   - FAIL = any check FAIL or MISSING

3. **Evidence must be fresh**
   - From current commit
   - Timestamped
   - Reproducible

4. **Zero external dependencies**
   - Uses only bash/grep/awk
   - No Python/Ruby/etc required

---

**Implementation**: COMPLETE ✅  
**Enforcement**: ACTIVE in CI ✅  
**Markdown guard**: ENFORCED ✅
