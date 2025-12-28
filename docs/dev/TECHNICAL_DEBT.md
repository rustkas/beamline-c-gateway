# Technical Debt Register

**Updated**: 2025-12-28T12:42:00+07:00

---

## P0 (Critical - Must Fix Before Production)

### ✅ RESOLVED

1. ✅ **set -e + ((VAR++)) bug** - FIXED in commit 4dceb0a
2. ✅ **Dual protocol in bench_memory.c** - FIXED in commit e3de43d8
3. ✅ **Fake artifacts (hardcoded exit codes)** - FIXED in commit e3de43d8

---

## P1 (High Priority - Should Fix Soon)

### 1. Machine-Readable Metrics: grep -P and stdout-as-API

**Issue**: Current metrics parsing uses grep -P (not portable) and treats stdout as API

**Problems**:
- grep -P not available in busybox/alpine
- stdout format changes break CI
- Regex parsing fragile

**Solution**:
- Benchmarks output JSON (last line or dedicated file)
- Wrapper collects JSON without regex
- Gate reads JSON with jq or simple parsing

**Status**: DOCUMENTED  
**File**: docs/dev/issues/MACHINE_READABLE_METRICS_RISK.md  
**Timeline**: Next iteration  
**User report**: Accurate critical analysis ✅

---

## P2 (Medium Priority - Nice to Have)

### 1. Makefile doesn't build benchmarks correctly

**Issue**: `make benchmarks` doesn't work, need manual gcc commands

**Status**: OPEN  
**Workaround**: Use manual gcc commands (documented)

### 2. PERF_ checks need actual implementation

**Issue**: PERF_ taxonomy defined but no actual PERF_ checks implemented

**Status**: OPEN  
**Blocked by**: Need to finalize threshold policies

---

## Technical Debt Summary

**P0 Resolved**: 3/3 ✅  
**P1 Open**: 1 (machine-readable metrics)  
**P2 Open**: 2 (Makefile, PERF_ implementation)

**Total**: 3 items remaining

---

## User Feedback Accuracy

All reported issues have been:
- ✅ Accurate
- ✅ Critical
- ✅ Properly prioritized

**User's technical review**: Invaluable! 🎯
