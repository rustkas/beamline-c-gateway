# DIAGNOSIS - Wrong Binary!

**Date**: 2025-12-27T16:04:00+07:00  
**Problem**: Using wrong binary for IPC gateway

---

## FINDING

`build/c-gateway` is **HTTP server**, not IPC server!

**Evidence**:
```c
// src/main.c
int main(void) {
    const char *port_env = getenv("GATEWAY_PORT");
    int rc = http_server_run(port_env);
    ...
}
```

This explains "Address already in use" - it's trying to bind HTTP port (8080), not Unix socket!

---

## CORRECT BINARY

Need **IPC server demo** binary, likely:
- `build/ipc-server-demo` (found earlier: PID 27423)
- Or similar IPC server binary

---

## FIX FOR EVIDENCE PACK

Update `GATEWAY_CMD`:

```bash
# Instead of:
GATEWAY_CMD="./build/c-gateway"

# Use:
GATEWAY_CMD="./build/ipc-server-demo /tmp/beamline-gateway.sock"

# Or find correct IPC binary:
find build -name '*ipc*' -type f -executable
```

---

## ACTION NEEDED

1. Find correct IPC server binary
2. Update evidence pack script
3. Re-run

---

Searching for IPC binaries...
