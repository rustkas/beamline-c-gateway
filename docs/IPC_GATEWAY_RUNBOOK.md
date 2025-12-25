# IPC Gateway Operations Runbook

## Overview
IPC Gateway provides Unix socket interface for IDE integrations to communicate with Router via NATS.

**Version**: 1.0  
**Status**: Production Ready  
**Components**: IPC Server, NATS Bridge, Protocol Handler

---

## Architecture

```
IDE Client → Unix Socket → IPC Server → NATS Bridge → Router
                ↓              ↓            ↓
            Protocol      Backpressure  Resilience
            Validation    Limits        Reconnect
```

---

## Quick Start

### Build
```bash
cd apps/c-gateway
mkdir -p build && cd build
cmake .. -DBUILD_IPC_GATEWAY=ON
cmake --build . --target ipc-server-demo ipc-nats-demo
```

### Run (Stub Mode)
```bash
# Terminal 1: Start server
export CGW_IPC_SOCKET_PATH=/tmp/beamline-gateway.sock
export CGW_IPC_NATS_ENABLE=0  # Stub mode
./build/ipc-nats-demo

# Terminal 2: Test client
python3 tests/test_ipc_client.py
```

### Run (Production Mode)
```bash
# With real NATS
export CGW_IPC_SOCKET_PATH=/tmp/beamline-gateway.sock
export CGW_IPC_NATS_ENABLE=1
export CGW_IPC_NATS_URL=nats://localhost:4222
export CGW_IPC_ROUTER_SUBJECT=beamline.router.v1.decide
export CGW_IPC_TIMEOUT_MS=30000
./build/ipc-nats-demo
```

---

## Configuration

### Environment Variables

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| CGW_IPC_ENABLE | bool | 1 | Enable IPC server |
| CGW_IPC_SOCKET_PATH | string | /tmp/beamline-gateway.sock | Socket path |
| CGW_IPC_MAX_CONNECTIONS | int | 64 | Max concurrent connections |
| CGW_IPC_NATS_ENABLE | bool | 0 | Enable NATS (0=stub) |
| CGW_IPC_NATS_URL | string | nats://localhost:4222 | NATS server URL |
| CGW_IPC_ROUTER_SUBJECT | string | beamline.router.v1.decide | Router subject |
| CGW_IPC_TIMEOUT_MS | int | 30000 | Request timeout |

### Validation
Config is validated on startup. Invalid values cause immediate exit with error.

---

## Health & Monitoring

### Health States
- **HEALTHY**: NATS connected, inflight < limits
- **DEGRADED**: NATS errors, high inflight
- **UNHEALTHY**: NATS down, overloaded

### Metrics
Check logs for:
- `event:request_received` - Request count
- `event:response_sent` - Response count, duration
- `event:error` - Error events
- `component:nats_resilience` - Connection state
- `component:ipc_backpressure` - Rejections

### JSONL Logs
All events logged as JSON Lines:
```json
{"timestamp":"2025-12-25T12:00:00.000Z","level":"INFO","event":"request_received","component":"ipc_bridge","request_id":"req_123","method":"task_submit","payload_size":1024}
```

---

## Troubleshooting

### Socket Permission Denied
```bash
# Check socket permissions
ls -la /tmp/beamline-gateway.sock
# Should be: srw------- (600)

# Fix: Remove old socket
rm /tmp/beamline-gateway.sock
```

### NATS Connection Failed
```bash
# Check NATS status
nc -zv localhost 4222

# Check config
echo $CGW_IPC_NATS_URL

# Enable debug
CGW_LOG_LEVEL=DEBUG ./ipc-nats-demo
```

### High Latency
```bash
# Check inflight count in logs
grep "inflight" logs.jsonl

# Check backpressure rejections
grep "overload" logs.jsonl

# Increase limits if needed
export CGW_IPC_MAX_CONNECTIONS=128
```

### Memory Growth
```bash
# Check RSS
ps aux | grep ipc-nats-demo

# Backpressure should prevent this
# If growing: check for inflight leaks in logs
```

---

## Common Operations

### Graceful Shutdown
```bash
# Send SIGTERM (handled by server)
kill -TERM <pid>

# Wait for inflight to drain (max 30s)
# Server closes socket and exits
```

### Restart
```bash
# Stop
systemctl stop ipc-gateway  # or manual SIGTERM

# Clean socket
rm -f /tmp/beamline-gateway.sock

# Start
systemctl start ipc-gateway
```

### Log Rotation
```bash
# Logs go to stderr (JSONL format)
# Redirect in systemd or use logger

# Example systemd:
StandardError=journal
```

---

## Dependencies

### Build Dependencies
- CMake >= 3.15
- GCC/Clang with C11 support
- jansson (JSON library)

### Runtime Dependencies
- NATS server (if CGW_IPC_NATS_ENABLE=1)
- Unix domain sockets support (Linux/macOS)

### No External Libraries Required
All IPC components use stdlib + POSIX APIs.

---

## Security

### Socket Permissions
- Default: 600 (owner read/write only)
- Socket owned by server process user
- No network exposure (local Unix socket)

### Secret Sanitization
- All logs sanitized (token/api_key/password masked)
- NATS credentials masked in config snapshot

### Recommendations
- Run as dedicated user (not root)
- Use filesystem permissions for access control
- Monitor for permission changes on socket

---

## Performance

### Benchmarks
- **Latency**: ~1-2ms (IPC) + NATS RTT
- **Throughput**: Limited by NATS, not IPC
- **Concurrent**: 64 connections default

### Limits
- **Inflight**: 1000 global, 10 per-connection
- **Payload**: 4MB max
- **Frame**: 4MB max

---

## Compliance

### Licenses
- IPC Gateway: Same as c-gateway project
- No additional dependencies

### SBOM
See `docs/DEPENDENCIES.md` (if applicable)

---

## Emergency Contacts

**On-call**: See team documentation  
**Escalation**: Router team  
**Documentation**: `docs/IDE_GATEWAY.md`

---

## Change Log

**v1.0** (2025-12-25):
- Initial production release
- Phase 1: IPC Server (Unix socket + binary protocol)
- Phase 2: NATS Integration
- Phase 3: Production readiness (backpressure, observability, resilience)
