# BENCHMARK_PLAN.MD - ASSESSMENT

**Date**: 2025-12-27T21:32:00+07:00  
**User claim**: "план и код не совпадают"  
**Verdict**: ✅ PARTIALLY CORRECT

---

## USER'S CLAIMS

### Claim 1: "throughput — однопоточный"

**Status**: ❌ **INCORRECT**

**Reality**: throughput HAS pthreads!
- Line 13: `#include <pthread.h>`
- Line 219: `pthread_t *threads = malloc(...)`
- Line 221: `pthread_create(&threads[i],...)`
- Line 192: `-t` flag for thread count
- Line 25: `DEFAULT_THREADS 4`

**Code DOES match plan**: "Spawn multiple client threads" ✅

---

### Claim 2: "memory bench — не код IPC вообще"

**Status**: ⚠️ **WAS TRUE, NOW FIXED**

**Old code** (before our fix): Just instructions
**New code** (after our fix): Real IPC benchmark!
- Uses ipc_protocol framing
- Connects to socket
- Sends requests
- Measures RSS/FD

**Code NOW matches plan**: "Track RSS under load" ✅

---

## BUT PLAN IS STILL OUTDATED

**User is RIGHT that plan needs updating**:

### Missing from plan:

1. ❌ **Real IPC protocol framing**
   - Plan doesn't mention ipc_protocol.h
   - Plan doesn't mention frame format

2. ❌ **Canonical socket path**
   - Plan doesn't mention /tmp/beamline-gateway.sock
   - Plan doesn't mention IPC_SOCKET_PATH env var

3. ❌ **Warmup phase**
   - Plan doesn't mention warmup requirement
   - Code has WARMUP_REQUESTS 100

4. ❌ **Payload size sweep**
   - Plan shows single payload
   - Code does 64/256/1024 sweep

5. ❌ **send_all/recv_all with EINTR**
   - Plan doesn't mention robust I/O
   - Code has proper error handling

6. ❌ **Timeouts**
   - Plan doesn't mention SO_RCVTIMEO/SO_SNDTIMEO
   - Code has 10 second timeouts

7. ❌ **Timestamped artifacts**
   - Plan shows simple v1.0_baseline.md
   - Code generates results/YYYYMMDD_HHMMSS/

---

## PLAN NEEDS UPDATE

**What to add**:

### Protocol Requirements
```markdown
## Protocol Contract

All benchmarks MUST:
- Use ipc_protocol.h framing: [len:4][ver:1][type:1][payload]
- Connect to canonical socket: /tmp/beamline-gateway.sock
- Override via IPC_SOCKET_PATH env var or -s flag
- Implement send_all/recv_all with EINTR/EAGAIN handling
- Set SO_RCVTIMEO/SO_SNDTIMEO timeouts
- Include warmup phase (100 requests)
- Support payload size sweep (64/256/1024 bytes)
```

### Artifact Format
```markdown
## Results Structure

results/YYYYMMDD_HHMMSS/
  throughput_64b.txt
  throughput_256b.txt
  throughput_1024b.txt
  latency_64b.txt
  latency_256b.txt
  latency_1024b.txt
  memory.txt
  summary.md
```

---

## VERDICT

**User's assessment**: ✅ PLAN OUTDATED (correct)

**User's specific claims**:
- "throughput однопоточный": ❌ WRONG (has pthreads)
- "memory не IPC": ⚠️ WAS TRUE, NOW FIXED

**Action needed**: Update BENCHMARK_PLAN.md to match reality

---

**Status**: Plan needs refresh  
**User catch**: Good (plan is outdated)  
**Accuracy**: 50% (wrong on throughput threads, right on plan being stale)
