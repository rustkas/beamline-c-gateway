# DIAGNOSIS - PONG Response Issue

**Date**: 2025-12-27T17:48:00+07:00

---

## GATEWAY BEHAVIOR

**From gateway.log**:
```
[handler] Received message type=240 payload_len=0
```

**Gateway received PING (type=240=0xF0)** ✅

**But**: No response logged, client timed out

---

## HANDLER CODE

**From ipc_server_demo.c line 38-52**:
```c
/* Echo back with success */
response->type = IPC_MSG_RESPONSE_OK;  // 0x10, not PONG!

char resp_buf[512];
int len = snprintf(resp_buf, sizeof(resp_buf),
    "{\"ok\":true,\"echo\":%s}",
    request->payload ? request->payload : "null");
```

**Gateway sends**: IPC_MSG_RESPONSE_OK (0x10) with JSON payload

**Client expects**: IPC_MSG_PONG (0xF1) for PING

---

## ROOT CAUSE

**Client recv_frame**:
```python
def recv_frame(sock: socket.socket) -> Tuple[int, int, bytes]:
    raw_len = recv_exact(sock, 4)
    (body_len,) = struct.unpack(">I", raw_len)
    # ...
```

**Problem**: Expects length as 4-byte field, but gateway sends full frame!

**Gateway sends** (ipc_protocol format):
```
[Length:4 BE] = total frame size
[Version:1]
[Type:1]
[Payload:N]
```

**Client reads**:
- First 4 bytes as "body_len" (should be full frame length)
- Then tries to read body_len bytes

**But**: First 4 bytes ARE the length field, not separate!

---

## FIX NEEDED

**recv_frame should read**:
1. First 4 bytes = total frame length (includes these 4 bytes!)
2. Then read (frame_length - 4) more bytes
3. Parse version,type from those bytes

---

Fixing recv_frame...
