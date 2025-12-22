# P0 NATS Integration: Quick Summary

---

## 🟢 **DECISION: DEFER C-GATEWAY INTEGRATION**

**Status**: C-Gateway will remain in **stub mode** until Router performance baseline is stable.

**Rationale**: Router perf regression investigation requires isolation. Adding c-gateway introduces variables that blur diagnostic signal.

**See**: ADR-005 in `.ai/decisions.md` for full decision rationale and prerequisites.

---

## 🔴 VERDICT: NO LIVE INTEGRATION - STUB MODE ACTIVE

### Key Findings

**1. Current Runtime Behavior:**
```
HTTP Client → c-gateway → nats_client_stub.c → FAKE JSON (hardcoded)
                                ↓
                        NO Router involved
                        NO NATS involved
```

**2. Code Quality:** ✅ **GOOD**
- Real NATS client implementation exists (`nats_client_real.c`)
- Correct subject: `beamline.router.v1.decide`
- Proper error handling skeleton
- BUT: Not compiled or linked

**3. Build Status:** ❌ **STUB BY DEFAULT**
```cmake
option(USE_NATS_LIB "Build with real NATS C client" OFF)  # ← This is the blocker
```

**4. Missing Dependencies:**
```bash
$ pkg-config --modversion nats
Package nats was not found
```
- `libnats` NOT installed on this system
- Headers NOT available
- Cannot build real NATS client without installing library

---

## What Responses Look Like Now (Stub Mode)

**Current `/api/v1/routes/decide` response:**
```json
{
  "ok": true,
  "result": {
    "decision": "allow",
    "route": "eu-west-1",
    "reason": "stub",        ← Hardcoded marker
    "message_id": "msg-stub-001",
    "trace_id": "trace-stub"
  }
}
```

**This response:**
- ✅ Is valid JSON
- ✅ Matches Router contract structure
- ❌ Is 100% fake (does not contact Router)
- ❌ Always returns "allow"
- ❌ Ignores actual request content

---

## Path Forward

### Option 1: Install NATS Library + Rebuild (Est: 30-60 min)

**Prerequisites:**
```bash
# Install libnats (Ubuntu/Debian)
sudo apt-get install -y cmake build-essential
git clone https://github.com/nats-io/nats.c.git
cd nats.c
mkdir build && cd build
cmake .. -DNATS_BUILD_STREAMING=OFF
make
sudo make install
sudo ldconfig
```

**Build c-gateway with NATS:**
```bash
cd /home/rustkas/aigroup/apps/c-gateway
mkdir -p build-nats
cd build-nats
cmake -DUSE_NATS_LIB=ON ..
make
```

**Test:**
```bash
# Requires: NATS server + Router running
export NATS_URL="nats://localhost:4222"
./build-nats/c-gateway
```

### Option 2: Use Existing Stub for Contract Testing (Est: 5 min)

**If Router not available yet**, validate OUTBOUND contract by:
1. Inspecting `build_route_request_json()` in http_server.c
2. Extracting actual JSON c-gateway sends
3. Running through Router's contract validator (offline)

**Advantage**: No infrastructure needed  
**Limitation**: Does not test end-to-end flow

---

## Architecture Issues (Real NATS Client)

Even after enabling NATS, current implementation has:

### 1. ❌ No Connection Pooling
**Current**: Opens new NATS connection per request  
**Impact**: +10-50ms latency per request  
**Fix**: Global connection pool (3-4 hours work)

### 2. ⚠️ Weak Error Handling
**Current**: All NATS errors → HTTP 503  
**Example**:
```c
// Everything becomes -1 → 503
if (s != NATS_OK) return -1;
```
**Should distinguish**:
- Timeout → 504 Gateway Timeout
- No responders → 503 Service Unavailable  
- Connection issues → 503

### 3. ❌ No Retry Logic
**Current**: Single attempt, immediate failure  
**Impact**: Transient network glitches cause user-visible errors  
**Fix**: Exponential backoff (1-2 hours work)

### 4. ❌ No Circuit Breaker
**Current**: Keeps hammering Router even if it's down  
**Impact**: Cascading failures, wasted resources  
**Fix**: Circuit breaker pattern (2-3 hours work)

---

## Contract Status

### Subject Names: ✅ CORRECT

c-gateway uses:
- `beamline.router.v1.decide` ← Matches Router
- `beamline.router.v1.get_decision`
- `beamline.router.v1.admin.*`

### Payload Format: ⚠️ NEEDS VERIFICATION

**Outbound** (c-gateway → Router):
- Built by `build_route_request_json()` in http_server.c
- Appears to follow Router expectations
- **But**: No schema validation

**Inbound** (Router → c-gateway):
- Parsed by `map_router_error_status()` 
- Expects `{ok: bool, error: {...}, result: {...}}`
- **But**: No schema enforcement

**Recommendation**: Add JSON schema validation for both directions.

---

## Decision Points

### Q1: Is Router + NATS infrastructure available NOW?

**YES** → Go with Option 1 (install libnats, rebuild, integrate)  
**NO** → Go with Option 2 (validate contracts offline, defer live integration)

### Q2: What's the priority?

**P0: Prove end-to-end connectivity** → Focus on getting ONE request through  
**P1: Production hardening** → Add pooling, retries, circuit breakers  
**P2: Observability** → Add metrics, tracing, detailed logging

### Q3: What's the timeline?

**This week**: Aim for P0 (basic connectivity)  
**Next sprint**: Tackle P1 (production features)  
**Ongoing**: P2 (observability improvements)

---

## Immediate Next Steps

**Right now, you should:**

1. **Decide**: Do you have access to running Router + NATS?
   - If YES: Proceed with Option 1 (install + build)
   - If NO: Let's validate contracts offline first

2. **Check**: What's the Router deployment status?
   - Is it listening on NATS subjects?
   - What's the NATS URL?
   - Are CP1 or CP2 contracts active?

3. **Validate**: Review generated diagnostic
   - Read: `docs/P0_NATS_INTEGRATION_DIAGNOSTIC.md`
   - Confirm findings match your understanding
   - Identify any missing context

**Tell me:**
- Do you have Router + NATS running somewhere?
- What environment (local Docker, staging, other)?
- Should we install libnats and build real client NOW, or defer?
