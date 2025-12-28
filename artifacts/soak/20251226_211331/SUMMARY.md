# Soak Test with Metrics - Summary

**Date**: Пт 26 дек 2025 23:13:32 +07
**Duration**: 7201 seconds (120 minutes)
**Threads**: 8
**Exit Code**: 0

---

## Test Output

```
[7170s] acquired=96225453, released=96225445, errors=0, rate=13421/s, pool=24/32
[7180s] acquired=96361709, released=96361701, errors=0, rate=13421/s, pool=24/32
[7190s] acquired=96497659, released=96497651, errors=0, rate=13421/s, pool=24/32
[7200s] acquired=96632058, released=96632050, errors=0, rate=13421/s, pool=24/32
[worker] Thread exiting: acquired=12079741, released=12079741, errors=0
[worker] Thread exiting: acquired=12078476, released=12078476, errors=0
[worker] Thread exiting: acquired=12082894, released=12082894, errors=0
[worker] Thread exiting: acquired=12079331, released=12079331, errors=0
[worker] Thread exiting: acquired=12073045, released=12073045, errors=0
[worker] Thread exiting: acquired=12076038, released=12076038, errors=0
[worker] Thread exiting: acquired=12082425, released=12082425, errors=0
[worker] Thread exiting: acquired=12080108, released=12080108, errors=0

========================================
Soak Test Complete
========================================
Duration:           7200 seconds
Total Acquired:     96632058
Total Released:     96632058
Errors:             0
Pool Stats:
  Total buffers:    32
  Available:        32
  Pool acquired:    96632058
  Pool released:    96632058

Rate:               13421 ops/sec

✅ SUCCESS: No leaks, all buffers accounted for
[buffer_pool] Destroying pool (acquired=96632058, released=96632058)
```

---

## Resource Usage

RSS Statistics:
  Min: 1792 KB
  Max: 1792 KB
  Avg: 1792 KB
  Growth: 0.00%

FD Statistics:
  Min: 14
  Max: 14
  Avg: 14.0

Samples: 1426

---

## Artifacts

- Full output: `soak_output.log`
- Metrics CSV: `metrics.csv`
- Analysis: `analysis.txt`

---

## Verdict

✅ PASS - Soak test completed successfully

