# C-Gateway Implementation Plan: Step-by-Step Guide

**Current Focus**: GATEWAY-5 - Rate Limiting Enhancement  
**Timeline**: 3-4 hours  
**Target Completion**: 2025-01-15  
**Owner**: AGENT_4_C_GATEWAY

---

## Overview

This document provides a detailed step-by-step implementation plan for completing the C-Gateway rate limiting feature. It covers the transition from the current minimal implementation (50% complete) to a production-ready system (100% complete).

---

## Current State Analysis

### What's Implemented ✅
- Fixed window rate limiting for `/api/v1/routes/decide`
- Environment configuration: `GATEWAY_RATE_LIMIT_TTL_SECONDS`, `GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT`
- Basic 429 response on limit exceeded
- Single endpoint counter in `http_server.c`
- Basic test in `c-gateway-http-test.c`

### What's Missing ❌
- Rate limiting for other endpoints (`/api/v1/messages/*`, `/api/v1/registry/blocks/*`)
- Rate-limit response headers (`X-RateLimit-*`, `Retry-After`)
- Structured JSON error response with all required fields
- Metrics tracking for rate limit hits/exceeds
- Comprehensive unit tests
- Updated documentation

### Code Locations
```
src/http_server.c:
  - Line 391-460: rate_limit_check_routes_decide() implementation
  - Line 1342-1348: Rate limit enforcement in request handler
  
tests/c-gateway-http-test.c:
  - Line 182-209: test_post_routes_decide_rate_limit()
```

---

## Step-by-Step Implementation Plan

### Phase 1: Code Refactoring (45 minutes)

#### Step 1.1: Extract Rate Limit Configuration (15 min)
**Goal**: Centralize rate limit configuration in a reusable struct

**Actions**:
1. Create rate limit config struct:
```c
typedef struct {
    int ttl_seconds;
    int routes_decide_limit;
    int messages_limit;
    int registry_blocks_limit;
    int global_limit;
} rate_limit_config_t;
```

2. Initialize config from environment:
```c
static rate_limit_config_t rl_config = {0};

static void rate_limit_init_config(void) {
    rl_config.ttl_seconds = env_to_int("GATEWAY_RATE_LIMIT_TTL_SECONDS", 60);
    rl_config.routes_decide_limit = env_to_int("GATEWAY_RATE_LIMIT_ROUTES_DECIDE", 50);
    rl_config.messages_limit = env_to_int("GATEWAY_RATE_LIMIT_MESSAGES", 100);
    rl_config.registry_blocks_limit = env_to_int("GATEWAY_RATE_LIMIT_REGISTRY_BLOCKS", 200);
    rl_config.global_limit = env_to_int("GATEWAY_RATE_LIMIT_GLOBAL", 1000);
}
```

**Validation**:
- [ ] Compile successfully
- [ ] No behavioral changes to existing rate limiting

**Files Modified**:
- `src/http_server.c`

---

#### Step 1.2: Create Per-Endpoint Counters (15 min)
**Goal**: Track rate limits independently for each endpoint

**Actions**:
1. Define endpoint IDs (already exists, verify):
```c
typedef enum {
    ENDPOINT_ROUTES_DECIDE = 0,
    ENDPOINT_MESSAGES,
    ENDPOINT_REGISTRY_BLOCKS,
    ENDPOINT_MAX
} endpoint_id_t;
```

2. Create counter array:
```c
static unsigned int rl_counters[ENDPOINT_MAX] = {0};
static time_t rl_window_started_at = 0;
```

3. Create limit lookup function:
```c
static int get_endpoint_limit(endpoint_id_t endpoint) {
    switch (endpoint) {
        case ENDPOINT_ROUTES_DECIDE:
            return rl_config.routes_decide_limit;
        case ENDPOINT_MESSAGES:
            return rl_config.messages_limit;
        case ENDPOINT_REGISTRY_BLOCKS:
            return rl_config.registry_blocks_limit;
        default:
            return rl_config.global_limit;
    }
}
```

**Validation**:
- [ ] All endpoints have defined limits
- [ ] Compile successfully

**Files Modified**:
- `src/http_server.c`

---

#### Step 1.3: Generic Rate Limit Check Function (15 min)
**Goal**: Create reusable function for all endpoints

**Actions**:
1. Implement generic check:
```c
/* Returns: 0 = allowed, 1 = rate limit exceeded */
static int rate_limit_check(endpoint_id_t endpoint, const char *tenant_id, unsigned int *remaining_out) {
    rate_limit_init_config();
    
    time_t now = time(NULL);
    
    /* Reset window if expired */
    if (rl_window_started_at == 0 || (now - rl_window_started_at) >= rl_config.ttl_seconds) {
        rl_window_started_at = now;
        for (int i = 0; i < ENDPOINT_MAX; i++) {
            rl_counters[i] = 0;
        }
    }
    
    int limit = get_endpoint_limit(endpoint);
    unsigned int current = rl_counters[endpoint];
    
    if (current >= (unsigned int)limit) {
        if (remaining_out) *remaining_out = 0;
        return 1; /* Rate limit exceeded */
    }
    
    rl_counters[endpoint]++;
    if (remaining_out) *remaining_out = (unsigned int)limit - rl_counters[endpoint];
    return 0; /* Allowed */
}
```

2. Refactor existing code to use new function:
```c
/* Old code at line 1342: */
if (rate_limit_check_routes_decide() != 0) { ... }

/* New code: */
unsigned int remaining = 0;
if (rate_limit_check(ENDPOINT_ROUTES_DECIDE, ctx.tenant_id, &remaining) != 0) { ... }
```

**Validation**:
- [ ] Existing `/api/v1/routes/decide` rate limiting still works
- [ ] Unit test still passes

**Files Modified**:
- `src/http_server.c`

---

### Phase 2: Response Headers (30 minutes)

#### Step 2.1: Rate Limit Headers Helper (15 min)
**Goal**: Generate standard rate limit headers

**Actions**:
1. Create header generation function:
```c
static void add_rate_limit_headers(char *header_buf, size_t buf_size, 
                                     int limit, unsigned int remaining) {
    time_t reset_at = rl_window_started_at + rl_config.ttl_seconds;
    int retry_after = (int)(reset_at - time(NULL));
    if (retry_after < 0) retry_after = 0;
    
    snprintf(header_buf, buf_size,
        "X-RateLimit-Limit: %d\r\n"
        "X-RateLimit-Remaining: %u\r\n"
        "X-RateLimit-Reset: %ld\r\n"
        "Retry-After: %d\r\n",
        limit, remaining, (long)reset_at, retry_after);
}
```

**Validation**:
- [ ] Headers compile correctly
- [ ] Format matches spec

**Files Modified**:
- `src/http_server.c`

---

#### Step 2.2: Enhanced 429 Error Response (15 min)
**Goal**: Return structured JSON with all required fields

**Actions**:
1. Create enhanced error response function:
```c
static void send_rate_limit_error(int client_fd, endpoint_id_t endpoint, 
                                    const request_context_t *ctx) {
    int limit = get_endpoint_limit(endpoint);
    time_t reset_at = rl_window_started_at + rl_config.ttl_seconds;
    int retry_after = (int)(reset_at - time(NULL));
    if (retry_after < 0) retry_after = 0;
    
    /* Generate headers */
    char rl_headers[512];
    add_rate_limit_headers(rl_headers, sizeof(rl_headers), limit, 0);
    
    /* Generate JSON body */
    char body[1024];
    snprintf(body, sizeof(body),
        "{\n"
        "  \"error\": \"rate_limit_exceeded\",\n"
        "  \"message\": \"Too many requests\",\n"
        "  \"tenant_id\": \"%s\",\n"
        "  \"endpoint\": \"%s\",\n"
        "  \"retry_after_seconds\": %d\n"
        "}\n",
        ctx->tenant_id[0] ? ctx->tenant_id : "anonymous",
        get_endpoint_name(endpoint),
        retry_after);
    
    /* Send HTTP response */
    char response[2048];
    snprintf(response, sizeof(response),
        "HTTP/1.1 429 Too Many Requests\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "%s"
        "\r\n"
        "%s",
        strlen(body), rl_headers, body);
    
    write(client_fd, response, strlen(response));
}
```

2. Add endpoint name helper:
```c
static const char* get_endpoint_name(endpoint_id_t endpoint) {
    switch (endpoint) {
        case ENDPOINT_ROUTES_DECIDE: return "/api/v1/routes/decide";
        case ENDPOINT_MESSAGES: return "/api/v1/messages";
        case ENDPOINT_REGISTRY_BLOCKS: return "/api/v1/registry/blocks";
        default: return "unknown";
    }
}
```

**Validation**:
- [ ] 429 response contains all required fields
- [ ] Headers are correctly formatted
- [ ] JSON is valid

**Files Modified**:
- `src/http_server.c`

---

### Phase 3: Apply to All Endpoints (30 minutes)

#### Step 3.1: Identify Endpoints (5 min)
**Goal**: Map all public endpoints to rate limit checks

**Endpoints**:
- `POST /api/v1/routes/decide` → `ENDPOINT_ROUTES_DECIDE` ✅ (already implemented)
- `GET /api/v1/routes/decide/:messageId` → `ENDPOINT_ROUTES_DECIDE`
- `POST /api/v1/messages` → `ENDPOINT_MESSAGES`
- `GET /api/v1/messages/:id` → `ENDPOINT_MESSAGES`
- `POST /api/v1/registry/blocks/:type/:version` → `ENDPOINT_REGISTRY_BLOCKS`
- `PUT /api/v1/registry/blocks/:type/:version` → `ENDPOINT_REGISTRY_BLOCKS`
- `DELETE /api/v1/registry/blocks/:type/:version` → `ENDPOINT_REGISTRY_BLOCKS`

**Excluded**:
- `/_health` - no rate limiting
- `/_metrics` - no rate limiting
- `GET /api/v1/messages/stream` - SSE, separate handling

---

#### Step 3.2: Apply Rate Limiting (20 min)
**Goal**: Add rate limit checks to all endpoints

**Actions**:
For each endpoint in `http_server.c`, add check before processing:

```c
/* Example for POST /api/v1/messages */
if (strcmp(path, "/api/v1/messages") == 0 && strcmp(method, "POST") == 0) {
    unsigned int remaining = 0;
    if (rate_limit_check(ENDPOINT_MESSAGES, ctx.tenant_id, &remaining) != 0) {
        send_rate_limit_error(client_fd, ENDPOINT_MESSAGES, &ctx);
        close(client_fd);
        return;
    }
    
    /* Add headers to success response */
    /* ... existing logic ... */
}
```

**Pattern for all endpoints**:
1. Call `rate_limit_check()` before processing
2. On exceeded (return 1): call `send_rate_limit_error()` and return
3. On allowed (return 0): add headers to success response (optional for CP1)

**Validation**:
- [ ] All endpoints have rate limit checks
- [ ] No duplicate checks
- [ ] Compile successfully

**Files Modified**:
- `src/http_server.c` (multiple locations)

---

#### Step 3.3: Add Headers to Success Responses (Optional, 5 min)
**Goal**: Include rate limit headers in successful responses

**Actions**:
Modify success response helpers to include rate limit headers:

```c
/* Example: */
char success_response[8192];
char rl_headers[512];
add_rate_limit_headers(rl_headers, sizeof(rl_headers), limit, remaining);

snprintf(success_response, sizeof(success_response),
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: %zu\r\n"
    "%s"  /* Rate limit headers */
    "\r\n"
    "%s",
    strlen(json_response), rl_headers, json_response);
```

**Note**: This is optional for CP1, can defer to CP2

**Files Modified**:
- `src/http_server.c`

---

### Phase 4: Metrics (20 minutes)

#### Step 4.1: Add Metrics Tracking (15 min)
**Goal**: Track rate limit hits and exceeds

**Actions**:
1. Add metrics counters:
```c
static unsigned long rl_total_hits = 0;
static unsigned long rl_total_exceeded = 0;
static unsigned long rl_exceeded_by_endpoint[ENDPOINT_MAX] = {0};
```

2. Update `rate_limit_check()`:
```c
static int rate_limit_check(endpoint_id_t endpoint, const char *tenant_id, unsigned int *remaining_out) {
    /* ... existing logic ... */
    
    rl_total_hits++;
    
    if (current >= (unsigned int)limit) {
        rl_total_exceeded++;
        rl_exceeded_by_endpoint[endpoint]++;
        if (remaining_out) *remaining_out = 0;
        return 1;
    }
    
    /* ... rest of logic ... */
}
```

3. Add to `/_metrics` endpoint:
```c
/* In handle_metrics() */
snprintf(metrics_json, sizeof(metrics_json),
    "{\n"
    "  \"rps\": %.2f,\n"
    "  \"latency\": { \"p50\": %lu, \"p95\": %lu },\n"
    "  \"error_rate\": %.2f,\n"
    "  \"rate_limit\": {\n"
    "    \"total_hits\": %lu,\n"
    "    \"total_exceeded\": %lu,\n"
    "    \"exceeded_by_endpoint\": {\n"
    "      \"routes_decide\": %lu,\n"
    "      \"messages\": %lu,\n"
    "      \"registry_blocks\": %lu\n"
    "    }\n"
    "  },\n"
    "  \"nats\": \"%s\",\n"
    "  \"ts\": %ld\n"
    "}\n",
    /* ... existing metrics ... */
    rl_total_hits,
    rl_total_exceeded,
    rl_exceeded_by_endpoint[ENDPOINT_ROUTES_DECIDE],
    rl_exceeded_by_endpoint[ENDPOINT_MESSAGES],
    rl_exceeded_by_endpoint[ENDPOINT_REGISTRY_BLOCKS],
    /* ... rest ... */
);
```

**Validation**:
- [ ] Metrics increment correctly
- [ ] JSON format is valid
- [ ] Metrics endpoint returns updated data

**Files Modified**:
- `src/http_server.c`

---

### Phase 5: Testing (45 minutes)

#### Step 5.1: Unit Tests for All Endpoints (30 min)
**Goal**: Comprehensive test coverage

**Actions**:
1. Add test for `/api/v1/messages`:
```c
static void test_post_messages_rate_limit(void) {
    /* Set aggressive limit */
    setenv("GATEWAY_RATE_LIMIT_MESSAGES", "2", 1);
    
    char resp[4096];
    const char *req_template =
        "POST /api/v1/messages HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: test-tenant\r\n"
        "Content-Length: 100\r\n"
        "\r\n"
        "{\"message_type\":\"chat\",\"payload\":\"test\"}";
    
    /* First 2 requests should succeed */
    for (int i = 0; i < 2; i++) {
        int rc = send_http_request("127.0.0.1", 8080, req_template, resp, sizeof(resp));
        assert(rc == 0);
        assert(strstr(resp, "HTTP/1.1 200") != NULL || strstr(resp, "HTTP/1.1 201") != NULL);
    }
    
    /* Third request should hit rate limit */
    int rc = send_http_request("127.0.0.1", 8080, req_template, resp, sizeof(resp));
    assert(rc == 0);
    assert(strstr(resp, "HTTP/1.1 429") != NULL);
    assert(strstr(resp, "X-RateLimit-Limit: 2") != NULL);
    assert(strstr(resp, "Retry-After:") != NULL);
    assert(strstr(resp, "rate_limit_exceeded") != NULL);
}
```

2. Add test for registry blocks endpoint
3. Add test for rate limit headers validation
4. Add test for metrics endpoint

**Validation**:
- [ ] All tests pass
- [ ] Coverage includes success and error cases
- [ ] Headers are validated

**Files Modified**:
- `tests/c-gateway-http-test.c`

---

#### Step 5.2: Integration Test (10 min)
**Goal**: E2E test with real NATS

**Actions**:
Update `nats-router-smoke-test.c` to include rate limit checks:
```c
/* Test rate limiting in smoke test */
void test_rate_limit_integration(void) {
    /* Make rapid requests */
    /* Verify 429 after limit */
    /* Verify headers */
}
```

**Validation**:
- [ ] Smoke test passes with rate limiting enabled

**Files Modified**:
- `tests/nats-router-smoke-test.c`

---

#### Step 5.3: Manual Testing (5 min)
**Goal**: Verify with curl

**Actions**:
```bash
# Build and run
make -C apps/c-gateway
GATEWAY_RATE_LIMIT_ROUTES_DECIDE=3 ./apps/c-gateway/build/c-gateway &

# Test rapid requests
for i in {1..5}; do
  curl -X POST "http://localhost:8080/api/v1/routes/decide" \
    -H "Content-Type: application/json" \
    -H "X-Tenant-ID: test" \
    -d '{"message_id":"m-1","message_type":"chat","payload":"test"}' \
    -v
done

# Verify:
# - First 3: 200 OK with X-RateLimit-Remaining decreasing
# - Last 2: 429 with Retry-After header
```

**Validation**:
- [ ] Manual curl test passes
- [ ] Headers are correct
- [ ] JSON response is valid

---

### Phase 6: Documentation (30 minutes)

#### Step 6.1: Update Implementation Doc (15 min)
**Goal**: Document C11-specific implementation

**Actions**:
Update `docs/GATEWAY_RATE_LIMITING.md`:

```markdown
## C11 Implementation Notes (CP1)

The C-Gateway implements a simplified fixed-window rate limiter for CP1:

### Architecture
- **Algorithm**: Fixed window (in-memory)
- **Scope**: Process-local (single instance)
- **Storage**: Static arrays (no external dependencies)
- **Window Size**: Configurable via `GATEWAY_RATE_LIMIT_TTL_SECONDS` (default: 60s)

### Configuration
Environment variables:
- `GATEWAY_RATE_LIMIT_TTL_SECONDS=60` - window duration
- `GATEWAY_RATE_LIMIT_ROUTES_DECIDE=50` - limit for /api/v1/routes/decide
- `GATEWAY_RATE_LIMIT_MESSAGES=100` - limit for /api/v1/messages
- `GATEWAY_RATE_LIMIT_REGISTRY_BLOCKS=200` - limit for /api/v1/registry/blocks
- `GATEWAY_RATE_LIMIT_GLOBAL=1000` - fallback global limit

### Limitations (CP1)
- Not horizontally scalable (process-local counters)
- Fixed window algorithm (less precise than sliding window)
- No per-tenant quotas (CP2 feature)
- No Redis backend (CP2 feature)

### Example Usage
```bash
# Start gateway with custom limits
GATEWAY_RATE_LIMIT_ROUTES_DECIDE=10 ./c-gateway

# Test rate limiting
curl -X POST "http://localhost:8080/api/v1/routes/decide" \
  -H "Content-Type: application/json" \
  -d '{"message_id":"m-1","message_type":"chat","payload":"test"}'
```

### Migration to CP2
CP2 will introduce:
- Per-tenant rate limiting with JWT claims
- Redis-backed sliding window algorithm
- Horizontal scalability across instances
- Admin introspection API
```

**Files Modified**:
- `docs/GATEWAY_RATE_LIMITING.md`

---

#### Step 6.2: Update Developer Guide (10 min)
**Goal**: Add rate limiting examples to dev guide

**Actions**:
Update `docs/dev/C_GATEWAY_DEV_HOWTO.md`:

```markdown
### Rate Limiting

C-Gateway includes built-in rate limiting for all public endpoints.

**Quick test**:
```bash
# Set aggressive limit for testing
export GATEWAY_RATE_LIMIT_ROUTES_DECIDE=3
./apps/c-gateway/build/c-gateway

# Trigger rate limit (4th request will return 429)
for i in {1..4}; do
  curl -X POST "http://localhost:8080/api/v1/routes/decide" \
    -H "Content-Type: application/json" \
    -H "X-Tenant-ID: demo" \
    -d '{"message_id":"m-'$i'","message_type":"chat","payload":"test"}' \
    -i | grep -E "HTTP|X-RateLimit|Retry-After"
done
```

**Expected output**:
```
Request 1: HTTP/1.1 200 OK, X-RateLimit-Remaining: 2
Request 2: HTTP/1.1 200 OK, X-RateLimit-Remaining: 1
Request 3: HTTP/1.1 200 OK, X-RateLimit-Remaining: 0
Request 4: HTTP/1.1 429 Too Many Requests, Retry-After: 60
```
```

**Files Modified**:
- `docs/dev/C_GATEWAY_DEV_HOWTO.md`

---

#### Step 6.3: Update README (5 min)
**Goal**: Add quick reference to main README

**Actions**:
Update `README.md` to reference C-Gateway:

```markdown
### C-Gateway (HTTP REST API)

C11-based HTTP gateway providing REST API for the Beamline platform.

**Quick Start**:
```bash
# Build
make -C apps/c-gateway

# Run (stub mode)
./apps/c-gateway/build/c-gateway

# Test
curl http://localhost:8080/_health
```

**Documentation**:
- [C-Gateway Developer Guide](docs/dev/C_GATEWAY_DEV_HOWTO.md)
- [Rate Limiting Spec](docs/GATEWAY_RATE_LIMITING.md)
- [ADR-016: C-Gateway Migration](docs/ADR/ADR-016-c-gateway-migration.md)
```

**Files Modified**:
- `README.md`

---

### Phase 7: Final Validation (20 minutes)

#### Step 7.1: Run All Tests (10 min)
**Actions**:
```bash
cd apps/c-gateway/build

# Run all unit tests
./c-gateway-json-test
./c-gateway-router-test
./c-gateway-nats-test
./c-gateway-http-test

# If real NATS available
./nats-router-smoke-test
```

**Validation**:
- [ ] All tests pass
- [ ] No memory leaks (valgrind clean)
- [ ] No warnings during compilation

---

#### Step 7.2: Code Review Checklist (5 min)
**Self-review**:
- [ ] All endpoints have rate limiting
- [ ] Response headers match spec
- [ ] JSON error format is correct
- [ ] Metrics are tracked
- [ ] Tests cover all cases
- [ ] Documentation is updated
- [ ] No TODO/FIXME comments left
- [ ] Memory is properly managed (no leaks)
- [ ] Error handling is consistent

---

#### Step 7.3: Update State Tracking (5 min)
**Actions**:
Update `.trae/milestones/cp1-completion.json`:
```json
{
  "id": "GATEWAY-5",
  "title": "Rate limiting",
  "status": "done",
  "progress": 1.0,
  "completed_at": "2025-01-15T00:00:00Z"
}
```

Update `apps/c-gateway/TODO.md`:
- Mark GATEWAY-5 as completed
- Move to CP2 planning

**Files Modified**:
- `.trae/milestones/cp1-completion.json`
- `apps/c-gateway/TODO.md`

---

## Summary Checklist

### Code Changes
- [ ] Rate limit config struct created
- [ ] Per-endpoint counters implemented
- [ ] Generic `rate_limit_check()` function
- [ ] Rate limit headers helper
- [ ] Enhanced 429 error response
- [ ] Rate limiting applied to all endpoints
- [ ] Metrics tracking added
- [ ] `/_metrics` endpoint updated

### Testing
- [ ] Unit tests for all endpoints
- [ ] Integration test updated
- [ ] Manual curl testing performed
- [ ] No regressions in existing tests

### Documentation
- [ ] `docs/GATEWAY_RATE_LIMITING.md` updated
- [ ] `docs/dev/C_GATEWAY_DEV_HOWTO.md` updated
- [ ] `README.md` updated
- [ ] Code comments added

### Validation
- [ ] All tests pass
- [ ] No compiler warnings
- [ ] No memory leaks (valgrind)
- [ ] State tracking updated

---

## Time Estimates

| Phase | Task | Time |
|-------|------|------|
| 1 | Code Refactoring | 45 min |
| 2 | Response Headers | 30 min |
| 3 | Apply to All Endpoints | 30 min |
| 4 | Metrics | 20 min |
| 5 | Testing | 45 min |
| 6 | Documentation | 30 min |
| 7 | Final Validation | 20 min |
| **Total** | | **3h 40min** |

---

## Success Criteria

✅ **Definition of Done for GATEWAY-5**:
1. Rate limiting works for all public endpoints
2. 429 responses include all required fields and headers
3. Metrics track hits and exceeds per endpoint
4. All tests pass (unit + integration)
5. Documentation updated with C11 examples
6. No memory leaks or compiler warnings
7. State tracking updated to reflect completion

---

## Next Steps After Completion

1. **Merge to main branch**
2. **Update CP1 milestone progress to 100%**
3. **Begin CP2 planning** (see `apps/c-gateway/TODO.md` for CP2 tasks)
4. **Schedule CP2 kickoff** with focus on JWT authentication

---

**Document Version**: 1.0  
**Last Updated**: 2025-11-22  
**Status**: Ready for implementation
