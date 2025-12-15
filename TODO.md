# C-Gateway Development TODO List

**Project**: Beamline Constructor - C-Gateway (HTTP REST API Gateway)  
**Status**: CP1-LC Completed (90%) → CP2-LC Planning  
**Last Updated**: 2025-11-22  
**Owner**: AGENT_4_C_GATEWAY

---

## 🎯 Current Priority: CP1 Completion (ETA: 2025-01-15)

### GATEWAY-5: Rate Limiting Enhancement (50% → 100%)
**Status**: 🔄 In Progress  
**Progress**: 50%  
**Estimated Effort**: 3-4 hours  
**Target Date**: 2025-01-15  
**Milestone**: CP1-LC

#### Acceptance Criteria
- [ ] **RL-1.1**: Extend rate limiting to all public endpoints
  - [ ] `/api/v1/messages/*` endpoints
  - [ ] `/api/v1/routes/decide` (already implemented)
  - [ ] `/api/v1/registry/blocks/*` endpoints
- [ ] **RL-1.2**: Add rate-limit response headers
  - [ ] `X-RateLimit-Limit: <limit>`
  - [ ] `X-RateLimit-Remaining: <remaining>`
  - [ ] `X-RateLimit-Reset: <epoch_seconds>`
  - [ ] `Retry-After: <seconds>` (429 responses)
- [ ] **RL-1.3**: Update 429 error response format
  ```json
  {
    "error": "rate_limit_exceeded",
    "message": "Too many requests",
    "tenant_id": "t-123",
    "endpoint": "/api/v1/messages",
    "retry_after_seconds": 60
  }
  ```
- [ ] **RL-1.4**: Add basic metrics counter
  - [ ] `gateway_rate_limit_exceeded_total` counter in `/_metrics`
- [ ] **RL-1.5**: Add unit tests for new endpoints
- [ ] **RL-1.6**: Update documentation
  - [ ] `docs/GATEWAY_RATE_LIMITING.md` - C11 implementation notes
  - [ ] `docs/dev/C_GATEWAY_DEV_HOWTO.md` - rate limiting examples

#### Implementation Steps
1. Extract rate limiting to reusable function: `rate_limit_check(endpoint_id, tenant_id)`
2. Add rate limit config struct with per-endpoint limits
3. Implement header generation function: `add_rate_limit_headers()`
4. Update error response to include all required fields
5. Add metrics tracking for exceeded limits
6. Write unit tests for all endpoints
7. Update documentation with C11 examples

#### Files to Modify
- `src/http_server.c`: Enhance rate limiting logic
- `src/nats_client_stub.h`: Add rate limit metrics struct (optional)
- `tests/c-gateway-http-test.c`: Add comprehensive rate limit tests
- `CMakeLists.txt`: No changes needed

---

## 🚀 CP2 Features (ETA: 2025-02-01 - 2025-02-28)

### Phase 1: Authentication & Authorization (Week 1-2)

#### GATEWAY-CP2-1: JWT Authentication
**Status**: 📅 Planned  
**Estimated Effort**: 2-3 days  
**Dependencies**: None  
**Priority**: High

**Tasks**:
- [ ] **AUTH-1.1**: Integrate JWT library for C11
  - Research: `libjwt` or `cjose` library
  - Add CMake dependency
- [ ] **AUTH-1.2**: Implement JWT validation function
  - Parse `Authorization: Bearer <token>` header
  - Validate signature, expiry, issuer
  - Extract `tenant_id`, `user_id`, `roles` claims
- [ ] **AUTH-1.3**: Add authentication middleware
  - Skip auth for `/_health`, `/_metrics`
  - Require auth for all `/api/v1/*` endpoints
- [ ] **AUTH-1.4**: Return 401 Unauthorized on invalid tokens
  ```json
  {
    "error": "unauthorized",
    "message": "Invalid or missing authentication token"
  }
  ```
- [ ] **AUTH-1.5**: Add unit tests for JWT validation
- [ ] **AUTH-1.6**: Update documentation

**Environment Variables**:
```bash
GATEWAY_AUTH_REQUIRED=true
GATEWAY_JWT_SECRET=<secret>
GATEWAY_JWT_ISSUER=beamline
GATEWAY_JWT_AUDIENCE=gateway
```

---

#### GATEWAY-CP2-2: Per-Tenant Rate Limiting
**Status**: 📅 Planned  
**Estimated Effort**: 1-2 days  
**Dependencies**: GATEWAY-CP2-1 (JWT auth for tenant_id)  
**Priority**: High

**Tasks**:
- [ ] **TENANT-RL-1**: Extract `tenant_id` from JWT claims or `X-Tenant-ID` header
- [ ] **TENANT-RL-2**: Implement per-tenant rate limit buckets
  - Key format: `tenant_id:endpoint_id:window_start`
  - Use hash table for per-tenant counters
- [ ] **TENANT-RL-3**: Add tenant-specific limits configuration
  ```bash
  GATEWAY_RATE_LIMIT_PER_TENANT_DEFAULT=100
  GATEWAY_RATE_LIMIT_TENANT_PREMIUM=1000
  GATEWAY_RATE_LIMIT_TENANT_BASIC=50
  ```
- [ ] **TENANT-RL-4**: Fallback to `anonymous` tenant for unauthenticated requests
- [ ] **TENANT-RL-5**: Add metrics with tenant label
  - `gateway_rate_limit_exceeded_total{endpoint,tenant}`
- [ ] **TENANT-RL-6**: Unit tests for multi-tenant rate limiting

---

### Phase 2: Observability & Metrics (Week 3)

#### GATEWAY-CP2-3: Prometheus Metrics
**Status**: 📅 Planned  
**Estimated Effort**: 1-2 days  
**Dependencies**: None  
**Priority**: Medium

**Tasks**:
- [ ] **METRICS-1**: Enhance `/_metrics` endpoint with Prometheus format
  - Current: JSON format
  - Add: Prometheus text format support (`Accept: text/plain`)
- [ ] **METRICS-2**: Add comprehensive metrics
  ```
  gateway_requests_total{method,endpoint,status}
  gateway_request_duration_seconds{method,endpoint}
  gateway_rate_limit_hits_total{endpoint,tenant}
  gateway_rate_limit_exceeded_total{endpoint,tenant}
  gateway_nats_publish_total{subject,status}
  gateway_nats_request_duration_seconds{subject}
  ```
- [ ] **METRICS-3**: Implement histogram for latency tracking
- [ ] **METRICS-4**: Add cardinality control
  - Tenant allowlist: `METRICS_TENANT_LABEL_ALLOWLIST`
  - Aggregate unlisted tenants as `tenant="other"`
- [ ] **METRICS-5**: Implement label support enhancement
  - [ ] Add `status` label to `metrics_record_http_request()` (line 131 in metrics_registry.c)
  - [ ] Add `tenant_id` label to `metrics_record_rate_limit_hit()` (line 145 in metrics_registry.c)
  - [ ] Add `subject` label to `metrics_record_nats_sent()` (line 162 in metrics_registry.c)
  - [ ] Add `subject` label to `metrics_record_nats_received()` (line 167 in metrics_registry.c)
  - [ ] Update Prometheus metrics format to include labels
- [ ] **METRICS-6**: Update documentation

**Environment Variables**:
```bash
GATEWAY_METRICS_FORMAT=prometheus  # json|prometheus|both
METRICS_TENANT_LABEL_ALLOWLIST=premium-1,premium-2
```

---

#### GATEWAY-CP2-4: OpenTelemetry Tracing
**Status**: 📅 Planned  
**Estimated Effort**: 2-3 days  
**Dependencies**: None  
**Priority**: Medium

**Tasks**:
- [ ] **OTEL-1**: Integrate OpenTelemetry C SDK
  - Research: `opentelemetry-cpp` or `opentelemetry-c`
  - Add CMake dependency
- [ ] **OTEL-2**: Implement tracing context extraction
  - Parse `traceparent` header (W3C Trace Context)
  - Fallback to `X-Trace-ID` header
- [ ] **OTEL-3**: Create spans for key operations
  - `http.request` - root span for HTTP request
  - `nats.publish` - NATS request span
  - `nats.reply` - NATS reply span
- [ ] **OTEL-4**: Add span attributes
  ```
  http.method, http.url, http.status_code
  nats.subject, nats.tenant_id
  trace_id, span_id, parent_span_id
  ```
- [ ] **OTEL-5**: Configure OTLP exporter
  ```bash
  OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4318
  OTEL_SERVICE_NAME=c-gateway
  ```
- [ ] **OTEL-6**: Unit tests for tracing

---

### Phase 3: Advanced Features (Week 4)

#### GATEWAY-CP2-5: Redis-backed Rate Limiting
**Status**: 📅 Planned  
**Estimated Effort**: 2-3 days  
**Dependencies**: GATEWAY-CP2-2  
**Priority**: Low (optional for CP2)

**Tasks**:
- [ ] **REDIS-RL-1**: Integrate hiredis (Redis C client)
- [ ] **REDIS-RL-2**: Implement sliding window algorithm
  - Second-level buckets: `rl:{tenant}:{endpoint}:{bucket_ts}`
  - Weighted sum over window
- [ ] **REDIS-RL-3**: Add Redis connection pool
- [ ] **REDIS-RL-4**: Fallback to in-memory if Redis unavailable
- [ ] **REDIS-RL-5**: Configuration
  ```bash
  GATEWAY_RATE_LIMIT_STORAGE=memory|redis
  GATEWAY_REDIS_URL=redis://localhost:6379
  ```
- [ ] **REDIS-RL-6**: Unit and integration tests

---

#### GATEWAY-CP2-6: SSE Enhancements
**Status**: 📅 Planned  
**Estimated Effort**: 1 day  
**Dependencies**: GATEWAY-CP2-1 (JWT auth)  
**Priority**: Low

**Tasks**:
- [ ] **SSE-1**: Add authentication for SSE streams
- [ ] **SSE-2**: Implement heartbeat/keepalive
  - Send `:keepalive\n\n` every 30 seconds
- [ ] **SSE-3**: Add connection metrics
  - `gateway_sse_active_connections{tenant}`
- [ ] **SSE-4**: Graceful disconnect handling
- [ ] **SSE-5**: Unit tests

---

#### GATEWAY-CP2-7: Admin Introspection API
**Status**: 📅 Planned  
**Estimated Effort**: 1-2 days  
**Dependencies**: GATEWAY-CP2-1, GATEWAY-CP2-2  
**Priority**: Low (optional for CP2)

**Tasks**:
- [ ] **ADMIN-1**: Implement admin endpoints
  - `GET /api/v1/admin/rate-limit/usage?tenant=<id>`
  - `GET /api/v1/admin/health/detailed`
  - `GET /api/v1/admin/metrics/tenants`
- [ ] **ADMIN-2**: Add RBAC for admin endpoints
  - Require `admin` role in JWT
- [ ] **ADMIN-3**: Return comprehensive status
  ```json
  {
    "tenant_id": "t-123",
    "endpoints": {
      "/api/v1/messages": {
        "limit": 100,
        "remaining": 75,
        "reset_at": 1234567890
      }
    }
  }
  ```
- [ ] **ADMIN-4**: Unit tests

---

## 🧪 CP3: Load Testing & Performance (ETA: 2025-03-01 - 2025-03-15)

### GATEWAY-CP3-1: Performance Validation
**Status**: 📅 Planned  
**Estimated Effort**: 3-5 days  
**Dependencies**: CP2 completion  
**Priority**: Critical for CP3

**Tasks**:
- [ ] **PERF-1**: k6 load testing scenarios
  - Existing scripts: Ready at `k6/scripts/`
  - Scenario 1: Sustained 2000 RPS for 10 minutes
  - Scenario 2: Peak 4000 RPS for 1 minute
  - Scenario 3: Mixed endpoints (70% decide, 30% messages)
- [ ] **PERF-2**: Performance targets validation
  - [ ] Throughput: ✅ 2000 RPS sustained, ✅ 4000 RPS peak
  - [ ] Latency: ✅ p50 < 10ms, ✅ p99 < 50ms
  - [ ] Memory: ✅ RSS < 200MB under load
  - [ ] CPU: ✅ < 50% on single core at 2000 RPS
- [ ] **PERF-3**: Profiling and optimization
  - Run valgrind for memory leaks
  - Profile with gprof or perf
  - Optimize hot paths
- [ ] **PERF-4**: Generate performance report
  - Document results in `docs/dev/C_GATEWAY_PERFORMANCE_REPORT.md`
- [ ] **PERF-5**: Update `.trae/state.json` with validation results

---

## 📋 Documentation Tasks

### DOC-1: CP1 Documentation Updates
**Status**: 🔄 In Progress  
**Priority**: High

- [x] `docs/ADR/ADR-016-c-gateway-migration.md` - Migration decision
- [x] `docs/dev/C_GATEWAY_DEV_HOWTO.md` - Developer guide
- [ ] `docs/GATEWAY_RATE_LIMITING.md` - Add C11 implementation section
- [ ] `README.md` - Add C-Gateway section with quick start

---

### DOC-2: CP2 Documentation
**Status**: 📅 Planned  
**Priority**: Medium

- [ ] `docs/dev/C_GATEWAY_AUTH_GUIDE.md` - JWT authentication setup
- [ ] `docs/dev/C_GATEWAY_OBSERVABILITY.md` - Metrics and tracing guide
- [ ] `docs/dev/C_GATEWAY_DEPLOYMENT.md` - Production deployment guide
- [ ] `docs/ARCHITECTURE/api-registry.md` - Update with auth examples

---

### DOC-3: CP3 Documentation
**Status**: 📅 Planned  
**Priority**: Low

- [ ] `docs/dev/C_GATEWAY_PERFORMANCE_REPORT.md` - Load testing results
- [ ] `docs/dev/C_GATEWAY_TUNING_GUIDE.md` - Performance tuning guide

---

## 🐛 Technical Debt & Improvements

### DEBT-1: Code Quality
- [ ] Add static analysis with `cppcheck` or `clang-tidy`
- [ ] Add memory leak checks with `valgrind` to CI
- [ ] Improve error handling consistency
- [ ] Add more comprehensive logging

### DEBT-2: Build System
- [ ] Add automated Docker builds for multiple architectures (amd64, arm64)
- [ ] Create release artifacts (static binary, deb/rpm packages)
- [ ] Add version injection at build time

### DEBT-3: Testing
- [ ] Increase unit test coverage to >80%
- [ ] Add fuzzing tests for HTTP parsing
- [ ] Add stress tests for SSE connections
- [ ] Add chaos engineering tests (network failures, NATS unavailability)

---

## 📊 Success Metrics

### CP1 Success Criteria
- [x] Core endpoints implemented (100%)
- [x] NATS integration working (100%)
- [x] Basic tests passing (100%)
- [ ] Rate limiting complete (90% → 100%)
- [ ] Documentation updated (95% → 100%)

### CP2 Success Criteria
- [ ] JWT authentication working
- [ ] Per-tenant rate limiting enforced
- [ ] Prometheus metrics exposed
- [ ] OpenTelemetry tracing integrated
- [ ] All CP2 features documented

### CP3 Success Criteria
- [ ] Performance targets met (2000/4000 RPS)
- [ ] Load testing complete
- [ ] Production deployment smoke tests passed
- [ ] Performance report published

---

## 🔗 Dependencies & Integrations

### Upstream Dependencies
- **Router (Erlang/OTP)**: Receives NATS messages, provides responses
- **NATS Server**: Message bus for Gateway ↔ Router
- **JWT Issuer**: Authentication token provider (CP2)
- **Redis**: Rate limiting storage (CP2 optional)
- **OTLP Collector**: Tracing backend (CP2)

### Downstream Consumers
- **UI (SvelteKit)**: Consumes REST API
- **External Clients**: API consumers via HTTP
- **Monitoring Stack**: Prometheus, Grafana (Pre-Release)

---

## 📅 Timeline Summary

| Phase | Milestone | ETA | Status |
|-------|-----------|-----|--------|
| CP1 Completion | GATEWAY-5 | 2025-01-15 | 🔄 In Progress |
| CP2 Phase 1 | Auth & Tenant RL | 2025-02-10 | 📅 Planned |
| CP2 Phase 2 | Observability | 2025-02-20 | 📅 Planned |
| CP2 Phase 3 | Advanced Features | 2025-02-28 | 📅 Planned |
| CP3 | Load Testing | 2025-03-15 | 📅 Planned |

---

## 🚨 Blockers & Risks

### Current Blockers
- None

### Identified Risks
1. **JWT library selection for C11**: Limited mature options
   - **Mitigation**: Research libjwt vs cjose, proof-of-concept in Week 1
2. **OpenTelemetry C SDK complexity**: Large dependency
   - **Mitigation**: Consider lighter alternatives or custom implementation
3. **Redis integration overhead**: New dependency
   - **Mitigation**: Make optional for CP2, defer to CP3 if needed

---

## 📝 Notes
- C-Gateway is production-ready for CP1-LC
- Migration from TypeScript was successful (10x memory reduction)
- Focus on completing rate limiting before moving to CP2
- All CP2 features are incremental and non-breaking
- Load testing infrastructure already prepared (k6 scripts)

---

**Last Review**: 2025-11-22  
**Next Review**: 2025-01-15 (CP1 completion)
