# User's Benchmark Diffs - Reference Implementation

**Date**: 2025-12-27T10:35:00+07:00  
**Source**: User feedback (detailed git diffs)  
**Status**: Reference specification

---

## NOTE

Current benchmarks in repo ALREADY implement most requirements:
- ✅ DEFAULT_SOCKET_PATH "/tmp/beamline-gateway.sock"
- ✅ #include "ipc_protocol.h"
- ✅ ipc_encode_message() usage
- ✅ send_all/recv_all with EINTR
- ✅ -s option and IPC_SOCKET_PATH env
- ✅ Timeouts (SO_RCVTIMEO/SO_SNDTIMEO)

User's diffs represent "ideal reference" version.

**Action**: Save as specification, verify current code matches

---

## FILES

See User's message for complete diffs:

1. `bench_ipc_latency.c` - Full ipc_protocol version
2. `bench_ipc_throughput.c` - Full ipc_protocol version
3. `bench_memory.c` - Socket contract only
4. `load_test.sh` - Canonical socket path

---

**Status**: Saved as reference specification  
**Current Code**: Already implements most requirements  
**Focus**: Evidence pack (more critical)
