# BENCHMARK_PLAN.md - Unified Diff

**User's complete specification**

Apply with:
```bash
cd benchmarks
git apply < benchmark_plan.diff
```

---

## Key Changes

1. **Invariants section** - Protocol contract
   - Canonical socket path
   - Real IPC framing (not ad-hoc JSON)
   - Correct I/O (send_all/recv_all + EINTR)

2. **What is measured** - Facts only
   - Latency: RTT with warmup
   - Throughput: sustained req/s
   - Memory: RSS/FD stability

3. **Canonical parameters**
   - Socket, payload sizes, warmup, timeouts

4. **Build** - No extra libs
   - Link against ipc_protocol only

5. **Run** - Facts-only artifacts
   - Timestamped results/YYYYMMDD_HHMMSS/
   - meta.env, meta.git, command.txt
   - results.tsv (machine-readable)

6. **Acceptance**  
   - Use ipc_protocol framing
   - Default to canonical socket
   - Implement send_all/recv_all
   - Produce timestamped artifacts

---

## Results TSV Format

```
metric	value	unit	notes
latency_p50	...	us	payload=64
latency_p95	...	us	payload=64
latency_p99	...	us	payload=64
throughput	...	req_per_sec	payload=64
```

---

**See User's message for complete unified diff**
**Status**: Ready to apply
