# Smoke Test Documentation

## Overview

`scripts/smoke_cgateway_router.sh` is a **readiness tooling script** for validating the full c-gateway ↔ NATS ↔ Router stack.

**Current Status**: **Template Mode** (stub responses allowed by default)

---

## 🚨 Important: ADR-005 Compliance

Per **ADR-005** (`.ai/decisions.md`), c-gateway is currently in **STUB MODE**:
- No live NATS integration
- libnats NOT installed
- Real integration deferred until Router perf stable

This smoke script:
- ✅ **CAN** validate infrastructure (NATS, Router, C-Gateway all start)
- ✅ **CAN** check HTTP endpoints are reachable
- ❌ **CANNOT** validate real Router integration yet (would fail in strict mode)

---

## Usage

### Template Mode (Default) - Infrastructure Validation

Validates that all components start and respond, **accepts stub responses**:

```bash
# From c-gateway repo root
./scripts/smoke_cgateway_router.sh

# With explicit template mode flag
ALLOW_DUMMY_RESPONSE=1 ./scripts/smoke_cgateway_router.sh
```

**What it checks**:
- ✓ NATS starts and healthz responds
- ✓ Router starts (via release or rebar3 shell)
- ✓ C-Gateway starts and health endpoint responds
- ✓ `/api/v1/routes/decide` returns HTTP 200
- ⚠️ Allows stub/"dummy" responses (expected in current state)

**Exit codes**:
- `0` = PASS (infrastructure healthy)
- `2-13` = Infrastructure failure (see script for details)

### Strict Mode - Real Integration Validation

Requires **real Router responses** (will FAIL until integration complete):

```bash
ALLOW_DUMMY_RESPONSE=0 ./scripts/smoke_cgateway_router.sh
```

**Prerequisites** (see ADR-005):
1. ✅ Router perf stable (3+ green Heavy CT runs)
2. ✅ Performance baseline documented
3. ✅ Regression guards enabled in CI
4. ✅ `libnats` installed
5. ✅ C-Gateway rebuilt with `-DUSE_NATS_LIB=ON`

**Exit codes**:
- `0` = PASS (real integration working)
- `12` = Stub response detected (integration not ready)
- `2-11,13` = Other failures

---

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `ROUTER_ROOT` | `../otp/router` | Path to Router repository |
| `ARTIFACTS_DIR` | `_artifacts` | Where to store logs/artifacts |
| `NATS_HTTP_HOSTPORT` | `127.0.0.1:8222` | NATS monitoring endpoint |
| `CGW_HOSTPORT` | `127.0.0.1:8080` | C-Gateway HTTP listen address |
| `CGW_HEALTH_PATH` | `/_health` | C-Gateway health endpoint path |
| `CGW_DECIDE_PATH` | `/api/v1/routes/decide` | Decide endpoint path |
| `CGW_START_CMD` | auto-discover | C-Gateway start command |
| `CGW_START_ARGS` | `""` | Additional c-gateway arguments |
| `ROUTER_START_CMD` | auto-discover | Router start command |
| `ROUTER_HEALTH_URL` | (optional) | Router health check URL |
| `SMOKE_TIMEOUT_SECONDS` | `180` | Overall test timeout |
| `CURL_TIMEOUT_SECONDS` | `10` | HTTP request timeout |
| `ALLOW_DUMMY_RESPONSE` | `1` | Template mode: accept stubs |

### Custom Configuration Example

```bash
# Custom Router location
ROUTER_ROOT=/path/to/router \
ROUTER_START_CMD="/path/to/router/_build/default/rel/beamline_router/bin/beamline_router foreground" \
./scripts/smoke_cgateway_router.sh

# Custom c-gateway command
CGW_START_CMD="./build-nats/c-gateway" \
CGW_START_ARGS="--port 9090" \
./scripts/smoke_cgateway_router.sh
```

---

## Artifacts

All runs create timestamped artifact directories:

```
_artifacts/smoke_cgw_router_YYYYMMDD_HHMMSS/
├── smoke.log              # Main test log
├── env.txt                # Environment snapshot
├── nats_start.log         # NATS startup output
├── nats_status.log        # NATS status check
├── nats_stop.log          # NATS shutdown output
├── router.log             # Router full log
├── router.log.tail        # Router last 200 lines
├── router.pid             # Router process ID
├── cgw.log                # C-Gateway full log
├── cgw.log.tail           # C-Gateway last 200 lines
├── cgw.pid                # C-Gateway process ID
├── http_headers.txt       # Response headers from decide endpoint
├── http_body.json         # Response body from decide endpoint
└── http_code.txt          # HTTP status code
```

---

## CI Integration (Future)

Example GitLab CI job (template mode):

```yaml
smoke:cgateway:template:
  stage: integration
  script:
    - ./scripts/smoke_cgateway_router.sh
  artifacts:
    when: always
    paths:
      - _artifacts/smoke_cgw_router_*/
    expire_in: 7 days
  allow_failure: false  # Should pass in template mode
```

Example GitLab CI job (strict mode - NOT YET ENABLED):

```yaml
smoke:cgateway:strict:
  stage: integration
  script:
    - ALLOW_DUMMY_RESPONSE=0 ./scripts/smoke_cgateway_router.sh
  artifacts:
    when: always
    paths:
      - _artifacts/smoke_cgw_router_*/
    expire_in: 7 days
  allow_failure: true  # Will fail until prerequisites met
  rules:
    - if: '$ROUTER_PERF_STABLE == "true"'  # Only run when Router ready
```

---

## Troubleshooting

### "NATS healthz did not become ready"

**Cause**: NATS failed to start or is not listening on expected port

**Fix**:
```bash
# Check NATS logs
cat _artifacts/smoke_cgw_router_*/nats_start.log

# Verify NATS scripts exist
ls -la ${ROUTER_ROOT}/scripts/nats_*.sh

# Manually test NATS
${ROUTER_ROOT}/scripts/nats_start.sh
curl http://127.0.0.1:8222/healthz
```

### "Could not discover router start command"

**Cause**: Router not built or ROUTER_ROOT incorrect

**Fix**:
```bash
# Check Router build
ls -la ${ROUTER_ROOT}/_build/default/rel/*/bin/*

# Build Router if needed
cd ${ROUTER_ROOT}
rebar3 release

# Or specify manually
ROUTER_START_CMD="/path/to/router/bin/router foreground" \
./scripts/smoke_cgateway_router.sh
```

### "Could not discover c-gateway binary"

**Cause**: C-Gateway not built

**Fix**:
```bash
# Build c-gateway
cmake -S . -B build
cmake --build build

# Or specify manually
CGW_START_CMD="./build/c-gateway" \
./scripts/smoke_cgateway_router.sh
```

### "Response contains stub/dummy marker" (strict mode)

**Cause**: C-Gateway is in stub mode (expected until ADR-005 prerequisites met)

**Fix**:
```bash
# Run in template mode instead
ALLOW_DUMMY_RESPONSE=1 ./scripts/smoke_cgateway_router.sh

# OR wait for prerequisites, then:
# 1. Install libnats
# 2. Rebuild c-gateway with -DUSE_NATS_LIB=ON
# 3. Verify Router perf stable
# 4. Run strict mode
```

---

## Development Workflow

### Local Development Loop

1. **Make changes** to c-gateway code
2. **Rebuild**:
   ```bash
   cmake --build build
   ```
3. **Run smoke test** (template mode):
   ```bash
   ./scripts/smoke_cgateway_router.sh
   ```
4. **Check artifacts**:
   ```bash
   ls -lh _artifacts/smoke_cgw_router_*/
   tail -n 50 _artifacts/smoke_cgw_router_*/cgw.log
   ```

### Transitioning to Strict Mode

When ADR-005 prerequisites are met:

1. **Update documentation**:
   - Mark ADR-005 as superseded
   - Document integration completion

2. **Change default mode**:
   ```bash
   # In scripts/smoke_cgateway_router.sh, change line:
   allow_dummy="${ALLOW_DUMMY_RESPONSE:-0}"  # Default to strict
   ```

3. **Update CI**:
   - Enable strict mode job
   - Make smoke test blocking for promotion

---

## See Also

- `.ai/decisions.md` (ADR-005) - Integration deferral rationale
- `docs/P0_NATS_INTEGRATION_DIAGNOSTIC.md` - Technical analysis
- `docs/NEXT_STEPS.md` - Prerequisites and timeline
- `docs/P0_SUMMARY.md` - Executive summary
