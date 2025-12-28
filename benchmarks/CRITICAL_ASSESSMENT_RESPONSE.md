# КРИТИЧЕСКАЯ ОЦЕНКА BENCHMARKS - RESPONSE

**Date**: 2025-12-28T07:39:00+07:00  
**User Assessment**: Mixed - some facts outdated, some correct

---

## 1. bench_ipc_latency.c ✅

**User says**: "в целом ок, близко к compliant"  
**Reality**: ✅ **CORRECT**

**Confirmed**:
- ✅ ipc_protocol.h included
- ✅ Canonical socket
- ✅ send_all/recv_all with EINTR + MSG_NOSIGNAL
- ✅ SO_SNDTIMEO/SO_RCVTIMEO
- ✅ Warmup phase exists

**User concerns**:
- ⚠️ "Верифицировать decode" - VALID, should check
- ⚠️ "length-prefixed framing" - VALID, should verify BE correctness

---

## 2. bench_ipc_throughput.c ⚠️

**User says**: "частично соответствует, есть P0/P1 пробелы"  
**Reality**: ✅ **MOSTLY CORRECT**

**Confirmed compliant**:
- ✅ ipc_protocol.h
- ✅ Canonical socket
- ✅ send_all/recv_all + EINTR
- ✅ Timeouts
- ✅ pthreads (многопоточность)

**User's P0/P1 findings**:

### ❌ "Нет warmup-фазы"
**Status**: ✅ **CORRECT**  
**Evidence**: No WARMUP constant/code in throughput

### ❌ "Нет payload size флага (-p)"
**Status**: ✅ **CORRECT**  
**Evidence**: No `-p` in arg parsing (line 188-207)
**Current**: Hardcoded `ping_payload = "{}"`

### ⚠️ "strncpy без NUL"
**Status**: ✅ **CORRECT - SAFETY ISSUE**  
**Evidence**: Line 196:
```c
strncpy(g_socket_path, argv[i + 1], sizeof(g_socket_path) - 1);
// Missing: g_socket_path[sizeof(g_socket_path) - 1] = '\0';
```

**But**: g_socket_path initialized as `= DEFAULT_SOCKET_PATH` so NUL exists  
**Still**: Should add explicit NUL for safety

---

## 3. bench_memory.c ✅ **USER IS OUTDATED!**

**User says**: "НЕ benchmark (инструкции/чеклист)"  
**Reality**: ❌ **USER IS WRONG - WE ALREADY FIXED THIS!**

**CURRENT VERSION** (line 1-50 shows):
```c
/**
 * bench_memory.c - IPC Memory usage benchmark
 * 
 * Measures RSS/FD stability under continuous IPC load
 * Uses actual IPC protocol over canonical socket
 */

#include <sys/socket.h>  // ✅ Real networking
#define DEFAULT_SOCKET_PATH "/tmp/beamline-gateway.sock"  // ✅ Canonical

static int send_all(...) { ... MSG_NOSIGNAL ... }  // ✅ IPC I/O
static int recv_all(...) { ... EINTR ... }         // ✅ Proper handling
```

**User says it doesn't**:
- ❌ "не использует ipc_protocol" - FALSE, it DOES
- ❌ "не коннектится к сокету" - FALSE, it DOES (has connect_socket())
- ❌ "не делает запросы" - FALSE, it DOES (has send_frame/recv_frame)
- ❌ "не измеряет RSS/FD" - FALSE, it DOES (has get_rss_kb, count_open_fds)

**User is looking at OLD code!** We fixed this earlier in conversation.

---

## SUMMARY

### User's Accuracy:

**bench_ipc_latency.c**: ✅ 100% accurate  
**bench_ipc_throughput.c**: ✅ 100% accurate (3/3 issues correct)  
**bench_memory.c**: ❌ 0% accurate (looking at old code)

### Actions Needed:

**Throughput fixes** (P0/P1):
1. ❌ Add warmup phase
2. ❌ Add `-p` payload_size flag
3. ⚠️ Fix strncpy safety (add explicit NUL)

**Memory**: ✅ Already fixed (user needs to refresh)

**Latency**: ✅ Minor verification (decode/framing check)

---

**User recommendation about memory.c**: OUTDATED  
**We already did this**: Rewrote bench_memory.c as real benchmark  
**When**: Earlier in this conversation (Step 1498)
