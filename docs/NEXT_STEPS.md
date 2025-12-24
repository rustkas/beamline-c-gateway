# C-Gateway: Next Steps Quick Reference

**Last Updated**: 2025-12-22

---

## ❌ What NOT to Do Right Now

**DO NOT**:
- ❌ Install `libnats` library
- ❌ Build c-gateway with `-DUSE_NATS_LIB=ON`
- ❌ Try to connect c-gateway to Router
- ❌ Spend time on end-to-end integration testing
- ❌ Modify NATS client implementation

**Reason**: Router perf regression investigation requires isolation. See **ADR-005**.

---

## ✅ What IS Available Right Now

### Smoke readiness tooling (c-gateway ↔ Router)

Есть канонический smoke orchestrator:

- `scripts/smoke_cgateway_router.sh`

Режимы:
- Template mode (default): `ALLOW_DUMMY_RESPONSE=1` — ADR-005 safe, допускает stub.
- Strict mode: `ALLOW_DUMMY_RESPONSE=0` — требует реальной интеграции.

Артефакты пишутся в:
- `_artifacts/smoke_cgw_router_<timestamp>/`

**См.**: `docs/SMOKE_TEST.md` для полной документации.

---

## ✅ What TO Do Right Now

### 1. Keep Stub Mode Documented
- Current behavior is **intentional**
- Stub responses are **correct** for this phase
- Update docs if any changes to stub behavior

### 2. Validate Contracts Offline
```bash
# From Router repository
cd /home/rustkas/aigroup/apps/otp/router
./scripts/contract_check.py
```

### 3. Monitor Router Perf Investigation
Wait for Router team to:
- Stabilize Heavy CT tier (p95/p99 within thresholds)
- Establish performance baseline
- Enable regression guards in CI
- Confirm 3+ consecutive green runs

---

## 🔄 When to Revisit Integration

### Prerequisites (ALL must be met):

1. ✅ **Router perf stable**
   - Heavy CT tier: 3+ consecutive green runs
   - p95/p99 latency within accepted thresholds
   - Throughput variance < 5%

2. ✅ **Baseline documented**
   - Performance baseline in `docs/performance/baseline.md`
   - Accepted by team/stakeholders
   - Version tagged

3. ✅ **Regression guards active**
   - CI job running `router_performance_regression_SUITE`
   - Failures block promotion
   - Thresholds tuned and stable

4. ✅ **Environment stable**
   - CPU pinning implemented (taskset or cgroups)
   - No resource contention
   - Deterministic scheduling confirmed

### Next Task to Create

When prerequisites met, create:

**Task**: `T-PERF-E2E-01: Gateway ↔ Router End-to-End Performance`

**Scope**:
- Install libnats on target environment
- Build c-gateway with real NATS client
- Establish end-to-end latency baselines
- Add gateway-specific performance thresholds
- Validate CP2 contracts in live traffic
- Measure full request path: Client → Gateway → NATS → Router → NATS → Gateway → Client

**Estimated Effort**: 1-2 days

---

## 📂 Key Documentation

| Document | Purpose |
|----------|---------|
| `.ai/decisions.md` (ADR-005) | Decision rationale and prerequisites |
| `docs/P0_NATS_INTEGRATION_DIAGNOSTIC.md` | Technical deep dive (9.2KB) |
| `docs/P0_SUMMARY.md` | Executive summary with options |
| `.ai/current_state.md` | Current project status |

---

## 🎯 Current Focus Areas

### For C-Gateway Team

1. **Repository Hygiene** ✅ Complete
   - Documentation organized
   - Scripts automated
   - CI checks ready

2. **Contracts Validation** 🟡 Ongoing
   - CP2 contracts defined
   - Offline validation working
   - Waiting for Router stability

3. **Code Quality** 🟢 Maintenance
   - Keep stub implementation clean
   - Update tests as needed
   - Monitor for dependency updates

### For Router Team

**Critical Path** (blocks c-gateway integration):
1. Fix perf regression (Heavy CT tier)
2. Stabilize environment (CPU pinning, isolation)
3. Document baseline
4. Enable CI regression guards

---

## 📞 Coordination Points

### Before Starting Integration

**Confirm with Router team**:
- [ ] Performance baseline finalized?
- [ ] Regression guards enabled in CI?
- [ ] Environment stability confirmed?
- [ ] NATS subjects stable (no breaking changes planned)?
- [ ] CP2 contract version locked?

### Integration Handoff

**C-Gateway team needs from Router team**:
- NATS URL (staging/production)
- Subject names (confirm no changes from CP1 v1)
- Expected request/response schemas
- Timeout recommendations
- Error handling expectations
- Observability requirements (tracing, metrics)

---

## 🔧 Technical Debt to Address (When Integration Starts)

### Known Issues in `nats_client_real.c`

1. **No connection pooling** (opens connection per request)
   - Impact: +10-50ms latency overhead
   - Fix: Global connection pool (3-4 hours work)

2. **Weak error handling** (all errors → HTTP 503)
   - Impact: Poor error diagnostics
   - Fix: Map NATS error codes properly (1-2 hours)

3. **No retry logic** (single attempt only)
   - Impact: Transient failures visible to users
   - Fix: Exponential backoff (1-2 hours)

4. **No circuit breaker** (keeps hammering Router when down)
   - Impact: Cascading failures
   - Fix: Circuit breaker pattern (2-3 hours)

**Total Estimated Technical Debt**: 1-2 days of work

---

## ✅ Success Criteria (Future Integration)

When c-gateway integration is complete, we should have:

- [ ] Live NATS connection to Router
- [ ] Real request-reply working for `/api/v1/routes/decide`
- [ ] CP2 contracts validated in live traffic
- [ ] End-to-end latency < 50ms p95
- [ ] Error handling for all NATS error cases
- [ ] Connection pooling implemented
- [ ] Circuit breaker protecting Router
- [ ] Observability (metrics, tracing, logs)
- [ ] Integration tests passing
- [ ] Performance benchmarks documented

---

## 📈 Metrics to Track (Post-Integration)

### Latency
- p50 < 10ms (gateway overhead only)
- p95 < 25ms
- p99 < 50ms

### Availability
- 99.9% uptime
- Circuit breaker activations < 1/day
- Error rate < 0.1%

### Capacity
- 1000+ RPS per gateway instance
- Linear scaling with instances
- No memory leaks over 24h

---

**Questions?** See ADR-005 or P0 diagnostic documents.
