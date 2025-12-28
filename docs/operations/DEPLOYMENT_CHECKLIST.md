# Deployment Checklist - IPC Gateway v2.0

**Version**: 2.0  
**Ready for**: STAGING deployment

---

## Pre-Deployment Checklist

### ✅ Build Artifacts
- [x] All binaries compiled
- [x] Component tests pass
- [x] No compilation warnings
- [x] Git commit tagged: `957f579b5432e2af71fd841957089b231fd44ed4`

### ✅ Testing Complete
- [x] ASan strict mode: PASSED (4/4 components)
- [x] Valgrind: PASSED (0 leaks, 4 components)
- [x] 2-hour soak test: PASSED (96M ops)
- [x] Backpressure tests: PASSED (4/4)
- [x] Router contract validation: PASSED (3/3) ✅

### ✅ Documentation
- [x] README.md complete
- [x] CONTRIBUTING.md created
- [x] SECURITY.md created
- [x] Deployment guides ready

---

## Staging Deployment Steps

### Step 1: Environment Setup
- [ ] NATS cluster available
- [ ] Router deployed and running
- [ ] Network connectivity verified
- [ ] Monitoring in place

### Step 2: Deploy Gateway
- [ ] Upload build artifacts
- [ ] Set environment variables:
  - `IPC_SOCKET_PATH=/tmp/beamline-gateway.sock`
  - `NATS_URL=nats://[staging-nats]:4222`
- [ ] Start gateway service
- [ ] Verify socket created

### Step 3: Smoke Tests
- [ ] Basic connectivity test
- [ ] NATS connection verified
- [ ] Router communication works
- [ ] Health check responds

### Step 4: Router E2E Tests
- [ ] Run `e2e_router_subjects_headers.sh`
- [ ] Run `e2e_router_errors.sh`
- [ ] Run `e2e_router_timeouts.sh`
- [ ] Run `e2e_router_happy_path.sh 1000`
- [ ] Run `e2e_router_reconnect_storm.sh`

### Step 5: Validate Results
- [ ] All E2E tests passed
- [ ] No memory leaks detected
- [ ] Error handling correct
- [ ] Timeout behavior validated
- [ ] Reconnect stable

---

## Post-Deployment Validation

### Day 1
- [ ] Monitor for crashes
- [ ] Check memory usage
- [ ] Verify NATS connections stable
- [ ] Review error logs

### Week 1
- [ ] Run soak test (2+ hours)
- [ ] Check for memory leaks
- [ ] Validate performance metrics
- [ ] Review all error cases

---

## Rollback Plan

### If Critical Issues Found

**Immediate**:
1. Stop gateway service
2. Capture logs and core dumps
3. Document issue clearly

**Analysis**:
1. Review artifacts
2. Identify root cause
3. Create bug report

**Fix & Redeploy**:
1. Fix issue in dev
2. Re-run all tests
3. Redeploy to staging

---

## Success Criteria

### Staging Success
- [x] Gateway starts successfully
- [ ] Processes requests correctly
- [ ] No crashes for 24 hours
- [ ] All E2E tests pass
- [ ] Memory usage stable

### Production Gate
- [ ] All staging tests passed
- [ ] 7+ days stable in staging
- [ ] Performance validated
- [ ] Security reviewed
- [ ] Overall readiness: 80-85%+

---

**Current Status**: Ready for STAGING ✅  
**Next Gate**: Router E2E in staging → Production (80-85%)
