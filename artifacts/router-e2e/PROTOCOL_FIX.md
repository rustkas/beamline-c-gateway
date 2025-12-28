# PROTOCOL FORMAT ANALYSIS

**Date**: 2025-12-27T17:46:00+07:00

---

## ACTUAL IPC_PROTOCOL FORMAT

**From ipc_protocol.c line 43**:
```c
frame->length  = htonl((uint32_t)total_size);  // INCLUDES header!
frame->version = IPC_PROTOCOL_VERSION;
frame->type    = (uint8_t)msg->type;
```

**Correct format**:
```
[Length:4 BE] = total_size (INCLUDING 6-byte header)
[Version:1]   = 0x01
[Type:1]      = message type
[Payload:N]   = JSON
```

---

## INLINE PYTHON CLIENT ERROR

**Wrong** (current inline client):
```python
header = bytes([VERSION, msg_type])  # ver=0x01, type=0xF0
body = header + payload
frame = struct.pack(">I", len(body)) + body
# Length = len(ver + type + payload) = 2 + payload_len
```

**Correct** (test_ipc_client.py):
```python
frame_length = 6 + len(payload_bytes)  # TOTAL including header
frame = struct.pack('!I B B', frame_length, version, msg_type)
frame += payload_bytes
# Length = 6 + payload_len
```

---

## DIFFERENCE

**Inline client sends**:
- Length: `2 + payload_len` (excludes length field itself)

**Gateway expects**:
- Length: `6 + payload_len` (includes ALL header bytes)

**Result**: Gateway gets wrong length, fails decode

---

## FIX

Replace inline Python client encode_frame with:

```python
def encode_frame(msg_type: int, payload: bytes) -> bytes:
    frame_length = 6 + len(payload)  # TOTAL frame size
    frame = struct.pack('>I B B', frame_length, VERSION, msg_type)
    frame += payload
    return frame
```

---

**Status**: Root cause identified ✅
