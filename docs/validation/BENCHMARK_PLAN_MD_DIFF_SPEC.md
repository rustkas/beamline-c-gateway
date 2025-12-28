# BENCHMARK_PLAN.md - Complete Unified Diff

**User provided complete diff**

This diff:
- Removes CPU profiling / bench_cpu
- Removes run_all.sh references
- Adds canonical socket contract
- Adds ipc_protocol requirements
- Adds I/O correctness requirements
- Updates structure and usage examples
- Focuses on facts and artifacts

---

## Apply with:

```bash
cd /home/rustkas/aigroup/apps/c-gateway/benchmarks
patch -p1 < /path/to/this/diff
```

---

## Diff content (see User's message for complete unified diff)

Key sections updated:
1. Purpose statement (real protocol emphasis)
2. Canonical contract (socket + protocol + I/O)
3. What is benchmarked (3 items, not 4)
4. How to run (socket check + examples)
5. Output artifacts (timestamped with env.txt)
6. Notes (common pitfalls)

---

**Status**: Ready to apply
**File**: benchmarks/BENCHMARK_PLAN.md
