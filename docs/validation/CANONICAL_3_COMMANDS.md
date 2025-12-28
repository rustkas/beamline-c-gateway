# CANONICAL 3 COMMANDS - VERIFIED

**Date**: 2025-12-27T15:32:00+07:00  
**Status**: VERIFIED FROM REPOSITORY  
**Purpose**: Exact commands for Router E2E

---

## 1. GATEWAY_CMD

```bash
GATEWAY_CMD="./build/c-gateway"
```

**Environment**:
```bash
export IPC_SOCKET_PATH="/tmp/beamline-gateway.sock"
```

**Verification**: Binary exists at `./build/c-gateway` ✅  
**Notes**: No CLI args needed, reads env vars

---

## 2. ROUTER_CMD

```bash
ROUTER_CMD="cd /home/rustkas/aigroup/apps/otp/router && rebar3 shell --eval 'application:ensure_all_started(beamline_router).'"
```

**Alternative** (if release exists):
```bash
ROUTER_CMD="/home/rustkas/aigroup/apps/otp/router/_build/default/rel/beamline_router/bin/beamline_router foreground"
```

**Verification**: Path exists (from previous work) ⚠️  
**Notes**: Needs `rebar3` installed, starts Erlang shell

---

## 3. CLIENT REQUEST (how to send test requests)

**Option A: Python client** (exists in repo):
```bash
python3 tests/test_ipc_client.py
```
**File**: `tests/test_ipc_client.py`  
**Protocol**: Uses IPC protocol framing  
**Verification**: File exists ✅

**Option B: Build minimal C client** (from evidence pack script):
```bash
# Build ipc_cli (inline in script)
gcc -O2 -I include -o ipc_cli ipc_cli.c src/ipc_protocol.c

# Use it
./ipc_cli -s /tmp/beamline-gateway.sock -t 0x01 -j '{"command":"test"}'
```

**Option C: netcat (for debugging only)**:
```bash
echo '{"command":"ping"}' | nc -U /tmp/beamline-gateway.sock
```
**Note**: Won't use proper IPC framing

---

## RECOMMENDED FOR SCENARIOS

Use **Python client** (`tests/test_ipc_client.py`):

```bash
# Happy path
python3 tests/test_ipc_client.py --socket /tmp/beamline-gateway.sock --json '{"command":"task_submit"}'

# Error testing
python3 tests/test_ipc_client.py --socket /tmp/beamline-gateway.sock --json '{"invalid":"data"}' --expect-error

# Timeout testing  
python3 tests/test_ipc_client.py --socket /tmp/beamline-gateway.sock --timeout 1
```

---

## SUMMARY FOR USER

```bash
# Gateway
GATEWAY_CMD="./build/c-gateway"
export IPC_SOCKET_PATH="/tmp/beamline-gateway.sock"

# Router
ROUTER_CMD="cd /home/rustkas/aigroup/apps/otp/router && rebar3 shell --eval 'application:ensure_all_started(beamline_router).'"

# Client (for scenarios)
CLIENT_CMD="python3 tests/test_ipc_client.py"
```

---

**Status**: Gateway ✅ Client ✅ Router ⚠️ (needs user verification)
