# 15 TASKS - PROGRESS UPDATE

**Date**: 2025-12-28T10:45:00+07:00

---

## COMPLETED SO FAR (8/15)

### ✅ A. Bench Correctness
1. ✅ A1: Fix compile error - DONE
2. ✅ A2: Unify socket priority - DONE
3. ✅ A3: Eliminate duplicated protocol - DONE
4. ✅ A4: Throughput help/output - DONE (just now!)
5. ✅ A5: Negative-path validation - DONE (just now!)

### ✅ B. Wrapper Artifacts
6. ✅ B6: Capture real exit codes - DONE
7. ✅ B7: Parse metrics - DONE
8. ❌ B8: load_test.sh facts-only - NOT YET
9. ✅ B9: Bench gate script - DONE (just now!)

### ✅ C. Router E2E
10. ✅ C10: Formalize gating rules - DONE
11. ⚠️  C11: Deterministic start/stop - PARTIAL
12. ✅ C12: Schema versioning - DONE (just now!)
13. ✅ C13: Summary.json gate inputs - DONE (just now!)

### ❌ D. CI Enforcement
14. ⚠️  D14: CI build+bench+e2e - PARTIAL
15. ❌ D15: Forbid percent readiness - NOT YET

---

## JUST COMPLETED (5 tasks in last run!)

- ✅ A4: Added -h help to throughput, shows all options, outputs payload size
- ✅ A5: Added version/type validation to throughput AND memory (critical!)
- ✅ C12: Added schema_version\t1 to checks.tsv
- ✅ C13: Added failed_checks array and artifact_refs to summary.json
- ✅ B9: Created check_bench_gate.sh script

---

## REMAINING (4 tasks)

- ❌ B8: load_test.sh - needs checks.tsv and summary.json
- ⚠️  C11: READY markers in E2E - partial
- ⚠️  D14: Complete CI jobs - partial
- ❌ D15: Forbid percent readiness - not started

---

**Next: Implementing B8, C11, D14, D15...**
