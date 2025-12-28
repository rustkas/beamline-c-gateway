# SOLUTION - Gateway Already Running

**Date**: 2025-12-27T16:00:00+07:00  
**Root cause**: FOUND ✅

---

## PROBLEM

```
rustkas  27423 ... build/ipc-server-demo /tmp/beamline-gateway.sock
```

**ipc-server-demo** is already running on `/tmp/beamline-gateway.sock`

This is why c-gateway fails with "Address already in use"

---

## SOLUTION OPTIONS

### Option A: Use existing gateway
```bash
# Don't start new gateway, use existing one
# Modify script to skip gateway start if socket exists
```

### Option B: Kill existing gateway
```bash
kill 27423
# Then re-run evidence pack
```

### Option C: Use different socket
```bash
IPC_SOCKET_PATH=/tmp/e2e-test.sock ./tests/run_router_e2e_evidence_pack.sh
```

---

## RECOMMENDED: Option B

Kill existing demo, run proper evidence pack:

```bash
# Kill existing
ps aux | grep ipc-server-demo | grep -v grep | awk '{print $2}' | xargs kill

# Run evidence pack
./tests/run_router_e2e_evidence_pack.sh
```

---

## NEXT EXECUTION

After killing existing gateway, expect:
- ✅ SYS_GATEWAY_SOCKET PASS
- ✅ SYS_IPC_PING PASS  
- ? SYS_HAPPY_PATH (depends on Router responses)

---

**Action needed**: Kill PID 27423
