# 2 FACTS - CANONICAL START COMMANDS

**Date**: 2025-12-27T12:00:00+07:00  
**Purpose**: Answer User's request for exact start commands  
**Source**: Verified from repo

---

## 1. GATEWAY START COMMAND

**Binary**: `./build/c-gateway`

**Verified**: Yes (exists in build/ directory)

**Command**:
```bash
./build/c-gateway
```

**Notes**:
- Binary built via CMake (target: c-gateway)
- Reads socket path from environment: `IPC_SOCKET_PATH`
- Default socket: `/tmp/beamline-gateway.sock`
- No CLI args needed for socket path

**Example from ipc_nats_demo.c** (line 8):
```c
./ipc_nats_demo /tmp/beamline-gateway.sock 1   // Real NATS
```

But c-gateway binary uses env var instead.

---

## 2. ROUTER START COMMAND

**Location**: `/home/rustkas/aigroup/apps/otp/router`

**Status**: NOT VERIFIED (separate repo/directory)

**Best guess** (from User's previous context):
```bash
cd /home/rustkas/aigroup/apps/otp/router && rebar3 shell
```

**Alternative** (if release built):
```bash
/home/rustkas/aigroup/apps/otp/router/_build/default/rel/beamline_router/bin/beamline_router foreground
```

**Notes**:
- Router is Erlang/OTP application
- Uses rebar3 build tool
- May need `rebar3 release` first
- **NEEDS USER VERIFICATION**

---

## SUMMARY FOR SCRIPT

Current `run_router_e2e_evidence_pack.sh` uses:

**Gateway**:
- Autodiscovers `./build/c-gateway` ✅
- Works with env var `IPC_SOCKET_PATH` ✅

**Router**:
- Autodiscovers `/home/rustkas/aigroup/apps/otp/router` ✅
- Tries release binary or `rebar3 shell` ⚠️
- **NEEDS VERIFICATION FROM USER**

---

**Action needed from User**:

Please verify Router start command by running:
```bash
cd /home/rustkas/aigroup/apps/otp/router
ls -la _build/default/rel/*/bin/
# OR
which rebar3 && rebar3 shell --help
```

Then provide exact command that works.
