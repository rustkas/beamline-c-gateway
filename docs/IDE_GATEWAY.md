# IPC Gateway Documentation

## Overview

C-Gateway IPC mode provides low-latency local communication between IDE clients (NeoVim, VSCode) and the Beamline Router via Unix domain sockets.

**Architecture**:
```
IDE/NeoVim → Unix Socket → IPC Server → NATS → Router
             (low latency)             (unified transport)
```

## Protocol

### Binary Framing

**Frame Format**:
```
[Length: 4 bytes][Version: 1 byte][Type: 1 byte][Payload: N bytes]
```

- **Length**: Total frame size (network byte order / big-endian)
- **Version**: Protocol version (currently `0x01`)
- **Type**: Message type (see below)
- **Payload**: JSON message (UTF-8 encoded)

### Message Types

| Type | Code | Description |
|------|------|-------------|
| `IPC_MSG_TASK_SUBMIT` | `0x01` | Submit new task |
| `IPC_MSG_TASK_QUERY` | `0x02` | Query task status |
| `IPC_MSG_TASK_CANCEL` | `0x03` | Cancel running task |
| `IPC_MSG_STREAM_SUBSCRIBE` | `0x04` | Subscribe to stream |
| `IPC_MSG_STREAM_DATA` | `0x05` | Streaming data chunk |
| `IPC_MSG_STREAM_COMPLETE` | `0x06` | Stream completed |
| `IPC_MSG_STREAM_ERROR` | `0x07` | Stream error |
| `IPC_MSG_RESPONSE_OK` | `0x10` | Success response |
| `IPC_MSG_RESPONSE_ERROR` | `0x11` | Error response |
| `IPC_MSG_PING` | `0xF0` | Ping (keepalive) |
| `IPC_MSG_PONG` | `0xF1` | Pong response |

### Error Codes

| Code | Name | Description |
|------|------|-------------|
| `0` | `IPC_ERR_OK` | No error |
| `1` | `IPC_ERR_INVALID_VERSION` | Unsupported protocol version |
| `2` | `IPC_ERR_INVALID_TYPE` | Unknown message type |
| `3` | `IPC_ERR_FRAME_TOO_LARGE` | Frame exceeds max size (4MB) |
| `4` | `IPC_ERR_INVALID_PAYLOAD` | Malformed JSON payload |
| `5` | `IPC_ERR_TIMEOUT` | Operation timed out |
| `6` | `IPC_ERR_CONNECTION_CLOSED` | Connection closed by peer |
| `99` | `IPC_ERR_INTERNAL` | Internal server error |

## Building

```bash
# Compile IPC protocol
gcc -c src/ipc_protocol.c -I include -o build/ipc_protocol.o

# Compile IPC server
gcc -c src/ipc_server.c -I include -o build/ipc_server.o

# Build demo
gcc examples/ipc_server_demo.c build/ipc_protocol.o build/ipc_server.o \
    -I include -o build/ipc_server_demo

# Build tests
gcc tests/test_ipc_protocol.c build/ipc_protocol.o \
    -I include -o build/test_ipc_protocol
```

## Running

### Start IPC Server (Demo)

```bash
./build/ipc_server_demo /tmp/beamline-gateway.sock
```

Output:
```
IPC Server Demo
===============

[ipc_server] Listening on Unix socket: /tmp/beamline-gateway.sock
Server ready. Press Ctrl+C to stop.
```

### Test with socat

```bash
# Simple echo test (manual framing)
echo '{"task":"hello"}' | socat - UNIX-CONNECT:/tmp/beamline-gateway.sock
```

**Note**: This won't work directly because the protocol expects binary framing. Use the Python client below for testing.

### Python Test Client

```python
#!/usr/bin/env python3
import socket
import struct
import json

def send_message(sock, msg_type, payload):
    """Send binary-framed IPC message"""
    payload_bytes = payload.encode('utf-8') if isinstance(payload, str) else payload
    frame_length = 6 + len(payload_bytes)  # header + payload
    version = 0x01
    
    # Pack frame: length(4) + version(1) + type(1) + payload(N)
    frame = struct.pack('!I B B', frame_length, version, msg_type) + payload_bytes
    sock.sendall(frame)

def recv_message(sock):
    """Receive binary-framed IPC message"""
    # Read header (6 bytes)
    header = sock.recv(6)
    if not header or len(header) < 6:
        return None
    
    length, version, msg_type = struct.unpack('!I B B', header)
    payload_len = length - 6
    
    if payload_len > 0:
        payload = sock.recv(payload_len).decode('utf-8')
    else:
        payload = None
    
    return {'type': msg_type, 'version': version, 'payload': payload}

# Connect
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect('/tmp/beamline-gateway.sock')

# Send TaskSubmit message
request = json.dumps({"task": "code_completion", "file": "test.py"})
send_message(sock, 0x01, request)  # IPC_MSG_TASK_SUBMIT

# Receive response
response = recv_message(sock)
print(f"Response type: 0x{response['type']:02x}")
print(f"Payload: {response['payload']}")

sock.close()
```

Save as `test_ipc_client.py` and run:
```bash
python3 test_ipc_client.py
```

## Integration with NATS

The IPC server message handler can forward requests to Router via NATS:

```c
void handle_message_with_nats(const ipc_message_t *request, 
                              ipc_message_t *response, 
                              void *user_data) {
    /* Forward to Router via NATS */
    char nats_response[8192];
    int rc = nats_request_decide(request->payload, nats_response, sizeof(nats_response));
    
    if (rc == 0) {
        /* Success */
        response->type = IPC_MSG_RESPONSE_OK;
        response->payload = strdup(nats_response);
        response->payload_len = strlen(nats_response);
    } else {
        /* Error */
        ipc_create_error_response(IPC_ERR_INTERNAL, "NATS request failed", response);
    }
}
```

## Security

### Filesystem Permissions

Unix sockets use filesystem permissions for access control:

```bash
# Default permissions: owner read/write only (600)
ls -l /tmp/beamline-gateway.sock
srw------- 1 user user 0 Dec 24 13:00 /tmp/beamline-gateway.sock
```

To allow group access:
```bash
chmod 660 /tmp/beamline-gateway.sock
chgrp developers /tmp/beamline-gateway.sock
```

### Multi-User Considerations

For single-user workstations (IDE use case):
- ✅ Default permissions (owner-only) are sufficient
- ✅ No authentication needed beyond filesystem ACLs

For shared development servers:
- ⚠️ Consider using separate sockets per user
- ⚠️ Or implement session-based authentication

## Performance

### Latency Benchmarks

**Unix Socket vs TCP Localhost**:
- Unix socket: ~1-2 μs (local IPC)
- TCP localhost: ~10-20 μs (loopback network)
- **Speedup**: ~10x faster for local communication

### Maximum Throughput

- **Frame size limit**: 4MB
- **Concurrent connections**: 64 (default)
- **Expected RPS**: 10,000+ (Unix socket bound, not application bound)

## Troubleshooting

### "Address already in use"

```bash
# Socket file exists from previous run
rm /tmp/beamline-gateway.sock

# Restart server
./build/ipc_server_demo
```

### "Permission denied"

```bash
# Check socket permissions
ls -l /tmp/beamline-gateway.sock

# Fix permissions
chmod 600 /tmp/beamline-gateway.sock
```

### "Connection refused"

```bash
# Verify server is running
ps aux | grep ipc_server_demo

# Check socket exists
ls -l /tmp/beamline-gateway.sock
```

## Next Steps

1. **Integration with NATS**: Connect IPC server to Router via NATS request-reply
2. **Streaming**: Implement chunked messages or JetStream for long-running tasks
3. **IDE Plugins**: Create NeoVim, VSCode clients using this protocol
4. **Observability**: Add logging, metrics, tracing

## See Also

- **ADR-006**: C-Gateway as IPC Gateway (`.ai/decisions.md`)
- **Task**: `.ai/task_cgw_ipc_gateway/`
- **Scope**: `.ai/task_cgw_ipc_gateway/scope.md`
