# User's Complete Git Diffs - Canonical Specification

**Date**: 2025-12-27T10:45:00+07:00  
**Source**: User's final detailed diffs  
**Status**: CANONICAL SPECIFICATION

---

## FILES

User provided complete git diffs for:

1. **bench_ipc_latency.c** - Full ipc_protocol implementation
2. **bench_ipc_throughput.c** - Full ipc_protocol implementation  
3. **load_test.sh** - Canonical socket + payload sweep

---

## KEY REQUIREMENTS (from diffs)

### Socket Path
```c
#define DEFAULT_SOCKET_PATH "/tmp/beamline-gateway.sock"
const char* env_socket = getenv("IPC_SOCKET_PATH");
// Priority: -s flag > IPC_SOCKET_PATH > DEFAULT
```

### Protocol
```c
#include "ipc_protocol.h"
ipc_encode_message(&msg, tx_buf, tx_cap, &frame_len);
ipc_decode_message(rx_buf, rx_len, &resp);
```

### I/O
```c
static int send_all(int fd, const uint8_t* buf, size_t len) {
    // Loop with EINTR/EAGAIN handling
    // MSG_NOSIGNAL
}

static int recv_all(int fd, uint8_t* buf, size_t len) {
    // Loop with EINTR/EAGAIN handling
}
```

### Timeouts
```c
set_socket_timeouts(fd, 10); // 10 seconds
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, ...);
setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, ...);
```

---

## CURRENT REPO STATE

**Current benchmarks ALREADY implement**:
- ✅ DEFAULT_SOCKET_PATH correct
- ✅ ipc_protocol.h included
- ✅ ipc_encode_message used
- ✅ send_all/recv_all with EINTR
- ✅ -s and IPC_SOCKET_PATH support
- ✅ Timeouts configured

**User's diffs = Reference ideal version**

---

## EVIDENCE PACK SPEC

User provided complete Router E2E evidence pack bash skeleton.

**Key requirement**: `results.json` as single source of truth

```json
{
  "timestamp": "20251227_101500",
  "git_commit": "abc123",
  "scenarios": [
    {"name":"happy_path","exit_code":0,"log":"..."}
  ]
}
```

**Readiness calculation**: Automatic from exit codes

---

**Status**: Saved as canonical specification  
**See**: User's message for complete diffs  
**Action**: Use as reference for any benchmark updates
