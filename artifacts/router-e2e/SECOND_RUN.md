# SECOND RUN - GATEWAY START ISSUE

**Date**: 2025-12-27T15:58:00+07:00  
**Run**: artifacts/router-e2e/20251227_155628/

---

## PROBLEM

Gateway не запускается - возможные причины:

1. **Env var not exported**: Script uses `IPC_SOCKET_PATH=${IPC_SOCKET_PATH} bash -c`
2. **Gateway needs explicit args**: Binary may not read env var
3. **Gateway crashes immediately**: Check stderr in gateway.log

---

## INVESTIGATION

Checking latest artifacts:
- command.txt - what commands were run
- gateway.log - gateway stderr/stdout
- meta.env - environment snapshot

---

## LIKELY FIX

Gateway binary may need explicit socket path argument:

```bash
# Instead of:
IPC_SOCKET_PATH="${IPC_SOCKET_PATH}" bash -c "${GATEWAY_CMD}"

# Use:
bash -c "${GATEWAY_CMD} --socket ${IPC_SOCKET_PATH}"
```

Or check if gateway has different CLI:
```bash
./build/c-gateway --help
```

---

Investigating...
