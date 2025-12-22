# P0 NATS Integration Diagnostic Report

**Generated**: 2025-12-22  
**Status**: 🔴 **STUB MODE - NO LIVE INTEGRATION**

---

## Executive Summary

C-Gateway **currently operates in STUB MODE** and has **NO live NATS integration with Router**. All responses are hardcoded stubs returning fake JSON.

### Critical Findings

1. ✅ **Code exists** for real NATS integration (`nats_client_real.c`)
2. ✅ **Correct subject** is configured: `beamline.router.v1.decide`
3. ❌ **CMake builds with stub by default** (`USE_NATS_LIB=OFF`)
4. ❌ **No NATS library linked** in standard builds
5. ❌ **All production traffic** goes through `nats_client_stub.c`

---

## P0.1: NATS Client Status - STUB Mode Confirmed

### Current Build Configuration

**CMakeLists.txt:36-93 Analysis:**

```cmake
option(USE_NATS_LIB "Build with real NATS C client" OFF)  # ← DEFAULT: OFF

if(USE_NATS_LIB)
    # Real NATS client (requires system libnats)
    add_executable(c-gateway
        src/nats_client_real.c  # ← This path exists
        ...
    )
    find_library(NATS_LIB nats)
    target_link_libraries(c-gateway PRIVATE ${NATS_LIB})
else()
    # STUB IMPLEMENTATION (default)
    add_executable(c-gateway
        src/nats_client_stub.c  # ← THIS IS CURRENT BUILD
        ...
    )
    # NO NATS LIBRARY LINKED
endif()
```

### What `nats_client_stub.c` Actually Does

**Source: `/home/rustkas/aigroup/apps/c-gateway/src/nats_client_stub.c:11-32`**

```c
int nats_request_decide(const char *req_json, char *resp_buf, size_t resp_size) {
    (void)req_json; /* unused for stub */
    
    // Returns HARDCODED fake JSON - DOES NOT CONTACT ROUTER
    const char *stub_response =
        "{\"ok\":true,\"result\":{"
        "\"decision\":\"allow\","
        "\"route\":\"eu-west-1\","
        "\"reason\":\"stub\","  // ← Explicit stub marker
        "\"message_id\":\"msg-stub-001\","
        "\"trace_id\":\"trace-stub\""
        "}}";
    
    strncpy(resp_buf, stub_response, resp_size - 1);
    return 0;  // Always succeeds
}
```

### Evidence: Stub References in Code

**Files using stub:**
- `src/http_server.c:5` - includes `nats_client_stub.h`
- `src/http_server.c:2514` - calls `nats_request_decide()` (stub version)
- `tests/c-gateway-nats-test.c` - validates stub behavior explicitly

**Call site**: `src/http_server.c:2514` (in `handle_decide`)
```c
int rc = nats_request_decide(route_req_json, resp_buf, sizeof(resp_buf));
// ↑ This currently calls stub - returns fake JSON
```

---

## P0.2: Subject Configuration - CORRECT ✅

### Router CP1 Subject Mapping

**Real NATS client knows the correct subject:**

`src/nats_client_real.c:14`
```c
static const char *DEFAULT_DECIDE_SUBJECT = "beamline.router.v1.decide";
```

**HTTP server uses same subject:**

`src/http_server.c:2508`
```c
subject = "beamline.router.v1.decide";
```

### Subject Usage Analysis

| Subject | Purpose | Status |
|---------|---------|--------|
| `beamline.router.v1.decide` | Main decision endpoint | ✅ Correct |
| `beamline.router.v1.get_decision` | Get by message_id | ✅ Correct |
| `beamline.router.v1.admin.*` | Admin endpoints | ✅ Correct |

**Verdict**: Subject names match Router expectations. **No contract mismatch**.

---

## P0.3: Real NATS Client Implementation Review

### Connection Management

**Source: `src/nats_client_real.c:24-121`**

**Architecture**:
- ❌ **Opens NEW connection per request** (lines 65, 76)
- ❌ **No connection pooling**
- ⚠️ **Timeout**: 5000ms (configurable via `ROUTER_REQUEST_TIMEOUT_MS`)
- ✅ **Proper cleanup** (destroys connection after each request)

**Performance Impact**: Opening NATS connection per request adds ~10-50ms latency overhead.

### Request-Reply Pattern

**Implementation:**
```c
s = natsConnection_RequestString(&reply,
                                conn,
                                subject,
                                req_json,
                                (int)strlen(req_json),
                                timeout_ms);  // 5000ms default
```

✅ **Correct**: Uses synchronous request-reply  
✅ **Timeout handling**: Returns error on timeout  
❌ **No retry logic**: Single attempt only  
❌ **No backpressure handling**: Sends regardless of Router load

### Error Handling

**From: `src/nats_client_real.c:68-89`**

```c
if (s != NATS_OK) {
    fprintf(stderr, "[c-gateway] nats connect error: %s\n", natsStatus_GetText(s));
    g_last_nats_status = "disconnected";
    return -1;  // ← Mapped to 503 by http_server.c
}
```

**Error Classes:**
1. **Connection failure** → -1 → HTTP 503
2. **Timeout** (NATS_TIMEOUT) → -1 → HTTP 503  
3. **No responders** (NATS_NO_RESPONDERS) → -1 → HTTP 503

❌ **Problem**: All errors become 503, no distinction between timeout/no-router/network-error

---

## P0.4: Integration Gaps

### What's Missing for Live Integration

#### 1. Build System
- Need to build with `-DUSE_NATS_LIB=ON`
- Need `libnats` installed on system
- Current Dockerfile likely builds stub version

#### 2. Configuration
Required environment variables (currently not set):
- `NATS_URL` (defaults to `nats://nats:4222`)
- `ROUTER_REQUEST_TIMEOUT_MS` (defaults to 5000)
- `ROUTER_DECIDE_SUBJECT` (optional override)

#### 3. NATS Infrastructure
- Need NATS server running
- Need Router subscribed to `beamline.router.v1.*` subjects
- Need network connectivity between c-gateway → NATS → Router

#### 4. Contract Validation
Currently **NO CP2 validation** in c-gateway:
- No schema validation on outbound requests
- No enforcement of required fields
- Relies on Router to validate (correct, but no early detection)

---

## P0.5: Recommended Next Steps

### Immediate Actions (next 30 minutes)

#### A. Verify NATS Library Availability
```bash
# Check if libnats is installed
pkg-config --modversion nats || echo "libnats NOT FOUND"

# If missing, install (Ubuntu/Debian)
sudo apt-get install -y libnats-dev
```

#### B. Build with Real NATS Client
```bash
cd /home/rustkas/aigroup/apps/c-gateway
mkdir -p build-nats
cd build-nats
cmake -DUSE_NATS_LIB=ON ..
make
```

#### C. Quick Smoke Test (if Router + NATS available)
```bash
# Terminal 1: Start c-gateway with NATS client
export NATS_URL="nats://localhost:4222"
export ROUTER_REQUEST_TIMEOUT_MS="3000"
./build-nats/c-gateway

# Terminal 2: Test decide endpoint
curl -X POST http://localhost:8080/api/v1/routes/decide \
  -H "Content-Type: application/json" \
  -H "X-Tenant-ID: test-tenant" \
  -d '{"from":"test@example.com","recipients":["user@example.com"],"subject":"test"}'

# Check response: should NOT contain "stub" in reason field
```

### Short-Term Improvements (next 2-4 hours)

#### 1. Add Connection Pooling
**Problem**: Creating connection per request is slow  
**Solution**: Global connection pool (5-10 connections)  
**Effort**: ~2-3 hours  
**Impact**: Reduces p95 latency by 20-40ms

#### 2. Enhanced Error Handling
**Problem**: All NATS errors → 503  
**Solution**: Map error types properly  
**Code**:
```c
if (s == NATS_TIMEOUT) return -2;        // → 504 Gateway Timeout
if (s == NATS_NO_RESPONDERS) return -3;  // → 503 Service Unavailable
if (s == NATS_CONNECTION_CLOSED) return -4; // → 503
```

#### 3. Add Diagnostic Logging
**What to log** (on every NATS call):
- Subject
- Request size
- Response size
- Latency (ms)
- NATS error code (if failed)
- Correlation ID (request_id)

**Why**: Essential for debugging Router integration issues

---

## Contract Compliance Status

### CP2 Contract Enforcement

**Current State**: ❌ **NO CP2 VALIDATION**

C-Gateway currently:
- ✅ Formats request JSON correctly (based on `build_route_request_json`)
- ✅ Uses correct subject names
- ❌ Does NOT validate outbound schema
- ❌ Does NOT validate inbound Router response schema
- ⚠️ Relies on Router for all validation (acceptable but risky)

### CP1 vs CP2

**Subject names suggest CP1** (`beamline.router.v1.decide`), but:
- Router contract docs mention CP2 adoption in progress
- C-Gateway needs alignment with Router's actual contract version

**Action Required**: Verify with Router team which contract version is in production.

---

## Decision Point

### Option A: Quick Enable (1-2 hours)
**Goal**: Get c-gateway talking to Router ASAP for integration testing

**Steps**:
1. Install libnats
2. Build with `-DUSE_NATS_LIB=ON`
3. Deploy with correct NATS_URL
4. Test basic decide flow
5. Accept connection-per-request inefficiency temporarily

**Risk**: Low (isolated to test environment)

### Option B: Production-Ready (1-2 days)
**Goal**: Properly architected NATS integration

**Steps**:
1. All of Option A
2. Add connection pooling
3. Add comprehensive error handling
4. Add diagnostic logging with correlation IDs
5. Add circuit breaker for Router unavailability
6. Add retry logic with exponential backoff
7. Add CP2 contract validation

**Risk**: Medium (more code, more testing needed)

---

## Conclusion

**Current Reality**: C-Gateway is **NOT** integrated with Router. All responses are fake.

**Good News**: Code for real integration exists and looks correct.

**Blocker**: CMake builds stub by default, need to rebuild with NATS library.

**Recommendation**: Start with **Option A** (Quick Enable) to unblock integration testing, then iterate toward Option B for production readiness.

**Next Command**: Run build diagnostic to check NATS library availability.
