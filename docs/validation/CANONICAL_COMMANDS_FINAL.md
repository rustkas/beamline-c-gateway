# CANONICAL COMMANDS - FINAL

**Date**: 2025-12-27T15:40:00+07:00  
**Status**: CONFIRMED BY USER

---

## GATEWAY COMMAND

```bash
GATEWAY_CMD="./build/c-gateway"
```

**Environment**:
```bash
export IPC_SOCKET_PATH="/tmp/beamline-gateway.sock"
```

---

## ROUTER COMMAND

**Option A** (CANONICAL - CHOSEN):
```bash
ROUTER_CMD="cd /home/rustkas/aigroup/apps/otp/router && rebar3 shell --eval 'application:ensure_all_started(beamline_router).'"
```

**Option B** (alternative, requires cwd):
```bash
cd /home/rustkas/aigroup/apps/otp/router
ROUTER_CMD="rebar3 shell --eval 'application:ensure_all_started(beamline_router).'"
```

---

## CLIENT COMMAND

```bash
CLIENT_CMD="python3 tests/test_ipc_client.py"
```

**Usage**:
```bash
python3 tests/test_ipc_client.py /tmp/beamline-gateway.sock
```

---

## USAGE IN EVIDENCE PACK

**Script**: `tests/run_router_e2e_evidence_pack.sh`

**Defaults** (can override via ENV):
- ROUTER_CMD: Option A (with cd)
- GATEWAY_CMD: ./build/c-gateway
- CLIENT: python3 tests/test_ipc_client.py

**Run**:
```bash
./tests/run_router_e2e_evidence_pack.sh
```

**Override**:
```bash
ROUTER_CMD="<custom>" ./tests/run_router_e2e_evidence_pack.sh
```

---

**Status**: ✅ ALL COMMANDS CONFIRMED
