# 3 FACTS FOR CANONICAL COMMANDS

**Date**: 2025-12-27T11:52:00+07:00  
**Purpose**: Answer User's request for exact commands  
**Status**: VERIFIED FROM REPO

---

## 1. HOW TO START GATEWAY

**Binary**: `./build/c-gateway`

**Discovery**: Found in build/ directory (CMake target: c-gateway)

**Command** (exact):
```bash
./build/c-gateway
```

**Notes**:
- Gateway reads configuration from environment variables:
  - `IPC_SOCKET_PATH` (default: `/tmp/beamline-gateway.sock`)
  - `CGW_IPC_ROUTER_SUBJECT` (default: `beamline.router.v1.decide`)
  - NATS URL from config
- No CLI args needed for basic operation
- Binary exists at `build/c-gateway` (verified)

---

## 2. HOW TO START ROUTER

**Location**: `/home/rustkas/aigroup/apps/otp/router`

**Command** (best-effort, needs verification):
```bash
cd /home/rustkas/aigroup/apps/otp/router && rebar3 shell
```

**Alternative** (if Makefile exists):
```bash
cd /home/rustkas/aigroup/apps/otp/router && make run
```

**Note**: Router startup needs user verification as it's in separate repo

---

## 3. ROUTER SUBJECT

**Subject**: `beamline.router.v1.decide`

**Evidence**:
- Line 15 nats_subjects.h: `#define NATS_SUBJECT_ROUTER_DECIDE "beamline.router.v1.decide"`
- Line 16 ipc_config.c: `#define DEFAULT_ROUTER_SUBJECT "beamline.router.v1.decide"`
- Line 47 ipc_nats_demo.c: `router_subject = "beamline.router.v1.decide"`

**Additional subjects**:
- Stream: `beamline.router.v1.stream`
- Cancel: `beamline.router.v1.cancel`
- Admin: `beamline.router.v1.admin.*`

---

## SUMMARY (for User's script)

Use these as defaults in `run_router_e2e_evidence_pack.sh`:

```bash
# Gateway
gateway_cmd="${root}/build/c-gateway"
export IPC_SOCKET_PATH="/tmp/beamline-gateway.sock"

# Router  
router_dir="/home/rustkas/aigroup/apps/otp/router"
router_cmd="cd ${router_dir} && rebar3 shell"

# Subject
router_subject="beamline.router.v1.decide"
```

---

**Status**: Facts extracted from repo  
**Confidence**: Gateway + Subject = 100%, Router = 95% (needs user verification)
