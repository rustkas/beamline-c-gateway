# Router E2E - Definition of Done

**Status**: MANDATORY FOR PRODUCTION  
**Gate**: ALL categories must show 100% PASS  
**Location**: Canonical production gate criteria

---

## PRODUCTION GATE

**Production deployment BLOCKED until**:

✅ ALL 5 categories below show **100% PASS**  
✅ Evidence collected for each test  
✅ No critical bugs found  
✅ Performance within SLOs  

---

## CATEGORY 1: Subjects/Headers Correctness

**Requirement**: Gateway ↔ Router messaging semantics correct

### Tests:

- [ ] Subject format matches Router contract (`beamline.router.v1.decide`)
- [ ] Request headers propagate to Router
- [ ] Response headers returned to client
- [ ] ReplyTo subject semantics work correctly
- [ ] Trace context (trace_id) preserved end-to-end
- [ ] Tenant isolation maintained

**Success Criteria**: 100% of tests pass

**Evidence Required**:
- NATS message captures showing correct subjects
- Header inspection logs
- Trace context verification

---

## CATEGORY 2: Error Semantics

**Requirement**: Router errors correctly translated to client

### Tests:

- [ ] 400 Bad Request from Router → Correct client error
- [ ] 404 Not Found from Router → Correct client error
- [ ] 500 Internal Server Error → Correct propagation
- [ ] 503 Service Unavailable (backpressure) → Recognized
- [ ] Unexpected error codes → No crashes, logged
- [ ] Error messages preserved

**Success Criteria**: 100% of error codes handled correctly

**Evidence Required**:
- Error response captures
- Gateway logs showing error handling
- No crashes on error scenarios

---

## CATEGORY 3: Timeout/Late Reply Handling

**Requirement**: No memory leaks or hangs on delayed responses

### Tests:

- [ ] Normal responses (<5s) processed correctly
- [ ] Slow responses (4-5s) handled
- [ ] Timeout (>5s) triggers cleanup
- [ ] Late reply (after timeout) doesn't leak memory
- [ ] Late reply doesn't cause double-free
- [ ] No callback hangs on timeout
- [ ] Memory check: 1000 timeouts = 0 leaks

**Success Criteria**: 
- 100% of timeout scenarios handled
- 0 memory leaks (ASan verification)
- 0 hangs (all requests complete)

**Evidence Required**:
- Timeout scenario logs
- ASan report after 1000 timeouts
- Resource monitoring (no FD/memory leaks)

**Risk Level**: **CRITICAL** (late replies commonly cause leaks)

---

## CATEGORY 4: Reconnect Storm with In-Flight

**Requirement**: Gateway survives NATS failures with active requests

### Tests:

- [ ] Single NATS disconnect/reconnect works
- [ ] In-flight requests during disconnect handled (not lost)
- [ ] Reconnect storm (10 rapid disconnect/reconnect cycles)
- [ ] Connection pool doesn't leak connections
- [ ] Pool size returns to normal after storm
- [ ] Failed reconnect attempts don't crash
- [ ] Requests resume after reconnect

**Success Criteria**:
- 100% of reconnect scenarios handled
- 0 connection leaks
- 0 lost requests (or explicit error returned)

**Evidence Required**:
- NATS connection logs
- Connection pool metrics
- Request success/failure rates
- Resource monitoring

**Risk Level**: **VERY HIGH** (connection leaks common)

---

## CATEGORY 5: Backpressure Handling

**Requirement**: Gateway responds appropriately to Router throttling

### Tests:

- [ ] Backpressure subject recognized (`beamline.router.v2.status.backpressure`)
- [ ] Gateway slows down on backpressure signal
- [ ] Gateway resumes on backpressure clear
- [ ] Circuit breaker triggers if appropriate
- [ ] No requests lost during backpressure
- [ ] Client receives appropriate 503 errors
- [ ] Metrics show backpressure events

**Success Criteria**: 
- Backpressure recognized 100%
- Appropriate client responses
- No request loss

**Evidence Required**:
- Backpressure signal logs
- Gateway behavior changes
- Client error responses
- Circuit breaker metrics

---

## OVERALL ACCEPTANCE CRITERIA

### All Tests Pass

**Required**:
- ✅ Category 1: 100% (6/6 tests)
- ✅ Category 2: 100% (6/6 tests)
- ✅ Category 3: 100% (7/7 tests) + 0 leaks
- ✅ Category 4: 100% (7/7 tests) + 0 leaks
- ✅ Category 5: 100% (7/7 tests)

**Total**: 33/33 tests PASS

---

### Performance SLOs

**Required**:
- p50 latency: <10ms
- p99 latency: <50ms
- p99.9 latency: <100ms
- Success rate: >99.9%
- Zero crashes in 24h test

---

### Evidence Complete

**Required**:
- All test logs captured
- ASan reports for leak tests
- NATS/Router communication logs
- Performance metrics
- Resource monitoring data

---

## EXECUTION PLAN

### Where to Execute:

**Staging Environment** (required):
- Real Router deployment
- Real NATS cluster
- Production-like load
- Full observability

**Cannot execute locally**: Router complexity requires proper environment

---

### Test Execution Order:

1. **Subjects/Headers** (functional baseline)
2. **Error Semantics** (error handling)
3. **Normal Load** (performance baseline)
4. **Timeout/Late Replies** (memory safety)
5. **Back pressure** (throttling)
6. **Reconnect Storm** (resilience)

---

### Timeline:

**Estimated**: 2-3 days in staging
- Day 1: Setup + Categories 1-2
- Day 2: Categories 3-4 (long-running)
- Day 3: Category 5 + evidence collection

---

## PRODUCTION GATE DECISION

### IF ALL PASS (33/33):

**System Integration**: 30-40% → **75-85%** ⬆️⬆️  
**Overall Readiness**: 65-70% → **80-85%** ⬆️⬆️  
**Decision**: **PRODUCTION APPROVED** ✅

---

### IF ANY FAIL:

**Action**: 
1. Document failures
2. Fix bugs
3. Re-test failed categories
4. Do NOT proceed to production

**Decision**: **PRODUCTION BLOCKED** ❌

---

## MAINTENANCE

**This Document**:
- Updated when requirements change
- Version controlled
- Referenced by TWO_AXIS_CANONICAL.md

**Changes Require**:
- Engineering review
- Update to staging plan
- Update to test scripts

---

**Status**: MANDATORY GATE  
**Categories**: 5  
**Total Tests**: 33  
**Pass Criteria**: 100% (33/33)  
**Location**: Staging environment  
**Timeline**: 2-3 days
