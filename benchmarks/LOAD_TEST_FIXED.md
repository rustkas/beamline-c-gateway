# LOAD_TEST.SH - CRITICAL BUG FIXED

**Date**: 2025-12-27T21:24:00+07:00  
**User Report**: ✅ ABSOLUTELY CORRECT - P0 bug!

---

## PROBLEM (Before Fix)

**Critical variable mismatch**:

```bash
# Line 14: DEFINED
IPC_SOCKET_PATH=${IPC_SOCKET_PATH:-/tmp/beamline-gateway.sock}

# Line 22, 26, 27: USED (UNDEFINED!)
echo "Socket:        ${SOCKET_PATH}"
if [ ! -S "$SOCKET_PATH" ]; then
    echo "Error: IPC socket not found at $SOCKET_PATH"
```

**Result**:
- ❌ `SOCKET_PATH` is **never defined**
- ❌ Socket check always fails (empty string)
- ❌ Benchmarks don't get socket path
- ❌ Script completely broken

**User assessment**: ✅ **100% CORRECT** - "реально сломан"

---

## FIX (After)

**Changed all references**:
```bash
# Line 14: DEFINED
IPC_SOCKET_PATH=${IPC_SOCKET_PATH:-/tmp/beamline-gateway.sock}

# Lines 22, 26, 27, 35, 39, 55, 61: USED (NOW CORRECT!)
echo "Socket:        ${IPC_SOCKET_PATH}"
if [ ! -S "$IPC_SOCKET_PATH" ]; then
    echo "Error: IPC socket not found at $IPC_SOCKET_PATH"
    
./build/bench-ipc-throughput -s "$IPC_SOCKET_PATH" ...
./build/bench-ipc-latency ... -s "$IPC_SOCKET_PATH"
```

**Fixed**:
- ✅ Variable name consistent throughout
- ✅ Socket check works
- ✅ Benchmarks get correct path via `-s`

---

## CHANGES MADE

1. **Line 22**: `${SOCKET_PATH}` → `${IPC_SOCKET_PATH}`
2. **Line 26**: `"$SOCKET_PATH"` → `"$IPC_SOCKET_PATH"`
3. **Line 27**: `$SOCKET_PATH` → `$IPC_SOCKET_PATH`
4. **Line 35**: Added `-s "$IPC_SOCKET_PATH"`
5. **Line 39**: Added `-s "$IPC_SOCKET_PATH"`
6. **Line 55**: Added `-s "$IPC_SOCKET_PATH"`
7. **Line 61**: Added `-s "$IPC_SOCKET_PATH"`

---

## SEVERITY

**Priority**: P0 🔥
- Script was completely non-functional
- Would fail on first socket check
- No way to run successfully

**User catch**: EXCELLENT ✅
- Spotted immediately
- Correct diagnosis
- Proper severity assessment

---

**Status**: FIXED ✅  
**User was right**: 100% correct, critical bug caught  
**File**: benchmarks/load_test.sh
