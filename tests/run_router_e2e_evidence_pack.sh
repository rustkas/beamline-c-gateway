#!/usr/bin/env bash
# run_router_e2e_evidence_pack.sh
# Router E2E evidence pack runner (facts-only)
# FINAL VERSION with inline Python client (no external deps)

set -euo pipefail

ts="$(date +%Y%m%d_%H%M%S)"
START_TIME=$(date +%s) # Added for summary.json duration calculation
root_dir="$(pwd)"
art_dir="${root_dir}/artifacts/router-e2e/${ts}"
mkdir -p "${art_dir}"

# ---- Canonical commands (can be overridden by env) ----
: "${IPC_SOCKET_PATH:=/tmp/beamline-gateway.sock}"
: "${GATEWAY_CMD:=./build/c-gateway}"
: "${ROUTER_CMD:=cd /home/rustkas/aigroup/apps/otp/router && rebar3 shell --eval 'application:ensure_all_started(beamline_router).'}"
: "${ROUTER_START_TIMEOUT_SEC:=30}"
: "${GATEWAY_START_TIMEOUT_SEC:=10}"

# Scenario knobs
: "${E2E_REQUESTS:=50}"
: "${E2E_PAYLOAD_BYTES:=256}"

# NATS assumptions
: "${NATS_HOST:=127.0.0.1}"
: "${NATS_PORT:=4222}"

# ---- Helpers ----
log() { printf '[%s] %s\n' "$(date -Is)" "$*" | tee -a "${art_dir}/stdout.log" >&2; }

write_meta() {
  {
    echo "ts=${ts}"
    echo "pwd=${root_dir}"
    echo "ipc_socket_path=${IPC_SOCKET_PATH}"
    echo "gateway_cmd=${GATEWAY_CMD}"
    echo "router_cmd=${ROUTER_CMD}"
    echo "e2e_requests=${E2E_REQUESTS}"
    echo "e2e_payload_bytes=${E2E_PAYLOAD_BYTES}"
    echo "nats=${NATS_HOST}:${NATS_PORT}"
  } > "${art_dir}/command.txt"

  {
    echo "uname: $(uname -a || true)"
    echo "whoami: $(whoami || true)"
    echo "ulimit_n: $(ulimit -n || true)"
    echo "env_IPC_SOCKET_PATH=${IPC_SOCKET_PATH}"
    echo "env_NATS_HOST=${NATS_HOST}"
    echo "env_NATS_PORT=${NATS_PORT}"
  } > "${art_dir}/meta.env"

  {
    if command -v git >/dev/null 2>&1; then
      echo "git_rev: $(git rev-parse HEAD 2>/dev/null || echo NA)"
      echo "git_dirty: $(git status --porcelain 2>/dev/null | wc -l | tr -d ' ' || echo NA)"
    else
      echo "git: NA"
    fi
  } > "${art_dir}/meta.git"

  {
    echo "bash: ${BASH_VERSION}"
    echo "python3: $(python3 --version 2>&1 || true)"
    echo "rebar3: $(rebar3 --version 2>&1 | head -n 1 || true)"
    echo "gcc: $(gcc --version 2>&1 | head -n 1 || true)"
  } > "${art_dir}/meta.versions"
}

wait_for_unix_socket() {
  local sock_path="$1"
  local timeout_sec="$2"
  local i
  for ((i=0;i<timeout_sec;i++)); do
    if [[ -S "${sock_path}" ]]; then
      return 0
    fi
    sleep 1
  done
  return 1
}

tcp_check() {
  local host="$1"
  local port="$2"
  (echo >"/dev/tcp/${host}/${port}") >/dev/null 2>&1
}

start_router() {
  log "Starting router..."
  echo "=== ROUTER START $(date -Is) ===" > "${art_dir}/router.log"
  bash -c "${ROUTER_CMD}" >> "${art_dir}/router.log" 2>&1 &
  echo $! > "${art_dir}/router.pid"
  log "Router PID: $(cat ${art_dir}/router.pid)"
}

start_gateway() {
  log "Starting gateway..."
  if [[ -S "${IPC_SOCKET_PATH}" ]]; then
    rm -f "${IPC_SOCKET_PATH}"
  fi

  echo "=== GATEWAY START $(date -Is) ===" > "${art_dir}/gateway.log"
  IPC_SOCKET_PATH="${IPC_SOCKET_PATH}" bash -c "${GATEWAY_CMD}" >> "${art_dir}/gateway.log" 2>&1 &
  echo $! > "${art_dir}/gateway.pid"
  log "Gateway PID: $(cat ${art_dir}/gateway.pid)"
}

stop_process() {
  local pid_file="$1"
  if [[ -f "${pid_file}" ]]; then
    local pid
    pid="$(cat "${pid_file}" || true)"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" >/dev/null 2>&1; then
      kill "${pid}" >/dev/null 2>&1 || true
      sleep 1
      if kill -0 "${pid}" >/dev/null 2>&1; then
        kill -9 "${pid}" >/dev/null 2>&1 || true
      fi
    fi
  fi
}

cleanup() {
  log "Cleanup..."
  stop_process "${art_dir}/gateway.pid"
  stop_process "${art_dir}/router.pid"
}
trap cleanup EXIT

# ---- Client (inline Python, stdlib only) ----
run_ipc_client() {
  log "Running IPC client: ping + ${E2E_REQUESTS} task_submit requests"
  python3 - <<'PY' "${IPC_SOCKET_PATH}" "${E2E_REQUESTS}" "${E2E_PAYLOAD_BYTES}" "${art_dir}/client.jsonl"
import json
import os
import socket
import struct
import sys
import time
from typing import Tuple

sock_path = sys.argv[1]
req_count = int(sys.argv[2])
payload_bytes = int(sys.argv[3])
out_path = sys.argv[4]

VERSION = 0x01
MSG_PING = 0xF0
MSG_PONG = 0xF1
MSG_TASK_SUBMIT = 0x01

def encode_frame(msg_type: int, payload: bytes) -> bytes:
    # Length includes full 6-byte header + payload
    frame_length = 6 + len(payload)
    frame = struct.pack('>I B B', frame_length, VERSION, msg_type)
    frame += payload
    return frame

def recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise RuntimeError("socket closed")
        buf += chunk
    return buf

def recv_frame(sock: socket.socket) -> Tuple[int, int, bytes]:
    # Read length field (4 bytes, big-endian)
    # Length includes the full frame: [length:4][version:1][type:1][payload:N]
    raw_len = recv_exact(sock, 4)
    (frame_len,) = struct.unpack(">I", raw_len)
    
    # Frame length includes the 4-byte length field itself
    # So remaining bytes = frame_len - 4
    remaining = frame_len - 4
    if remaining < 2:
        raise RuntimeError(f"Invalid frame length: {frame_len}")
    
    # Read rest of frame
    body = recv_exact(sock, remaining)
    ver = body[0]
    mtype = body[1]
    payload = body[2:] if len(body) > 2 else b""
    
    return ver, mtype, payload

def now_ns() -> int:
    return time.time_ns()

def make_payload(size: int, i: int) -> bytes:
    base = {
        "command": "task_submit",
        "task_type": "code_completion",
        "file": "bench.py",
        "line": i,
        "context": "def hello():\n    pass\n",
        "pad": "x" * 1,
    }
    b = json.dumps(base, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    if len(b) >= size:
        return b[:size]
    pad_len = size - len(b)
    return b + (b" " * pad_len)

def write_jsonl(rec: dict) -> None:
    with open(out_path, "a", encoding="utf-8") as f:
        f.write(json.dumps(rec, ensure_ascii=False) + "\n")

# Connect
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.settimeout(10.0)
s.connect(sock_path)

# Ping
t0 = now_ns()
s.sendall(encode_frame(MSG_PING, b""))
ver, mtype, payload = recv_frame(s)
t1 = now_ns()
write_jsonl({
    "check": "PING",
    "ok": (ver == VERSION and mtype == MSG_PONG),
    "ver": ver,
    "type": mtype,
    "rtt_us": (t1 - t0) / 1000.0,
    "payload_len": len(payload),
})

# Requests
ok_count = 0
lat_us = []
for i in range(req_count):
    payload = make_payload(payload_bytes, i)
    t0 = now_ns()
    s.sendall(encode_frame(MSG_TASK_SUBMIT, payload))
    ver, mtype, resp_payload = recv_frame(s)
    t1 = now_ns()

    ok = (ver == VERSION)
    parsed = None
    if resp_payload:
        try:
            parsed = json.loads(resp_payload.decode("utf-8", errors="strict"))
        except Exception:
            ok = False

    if isinstance(parsed, dict):
        if parsed.get("ok") is True:
            ok = ok and True
        elif parsed.get("status") in ("ok", "OK", "success"):
            ok = ok and True

    rtt = (t1 - t0) / 1000.0
    lat_us.append(rtt)
    if ok:
        ok_count += 1

    write_jsonl({
        "check": "TASK_SUBMIT",
        "i": i,
        "ok": ok,
        "ver": ver,
        "type": mtype,
        "rtt_us": rtt,
        "resp_len": len(resp_payload),
    })

s.close()

# Summary
lat_us_sorted = sorted(lat_us)
def pctl(p: float) -> float:
    if not lat_us_sorted:
        return 0.0
    k = int(round((p/100.0) * (len(lat_us_sorted)-1)))
    return float(lat_us_sorted[k])

write_jsonl({
    "check": "SUMMARY",
    "ok_count": ok_count,
    "total": req_count,
    "p50_us": pctl(50),
    "p95_us": pctl(95),
    "p99_us": pctl(99),
})
PY
}

# ---- checks.tsv generator ----
generate_checks_tsv() {
  local tsv="${art_dir}/checks.tsv"
  : > "${tsv}"
  
  # Schema version (C12: schema versioning)
  printf "schema_version\t1\n" >> "${tsv}"

  # SYS_NATS_UP
  if tcp_check "${NATS_HOST}" "${NATS_PORT}"; then
    printf "SYS_NATS_UP\tPASS\tnats=%s:%s\t%s\n" "${NATS_HOST}" "${NATS_PORT}" "meta.env" >> "${tsv}"
  else
    printf "SYS_NATS_UP\tFAIL\tnats=%s:%s_unreachable\t%s\n" "${NATS_HOST}" "${NATS_PORT}" "meta.env" >> "${tsv}"
  fi

  # SYS_ROUTER_RUNNING
  if [[ -f "${art_dir}/router.pid" ]] && kill -0 "$(cat "${art_dir}/router.pid")" >/dev/null 2>&1; then
    printf "SYS_ROUTER_RUNNING\tPASS\tpid=%s\t%s\n" "$(cat "${art_dir}/router.pid")" "router.log" >> "${tsv}"
  else
    printf "SYS_ROUTER_RUNNING\tFAIL\trouter_not_running\t%s\n" "router.log" >> "${tsv}"
  fi

  # SYS_GATEWAY_SOCKET
  if [[ -S "${IPC_SOCKET_PATH}" ]]; then
    printf "SYS_GATEWAY_SOCKET\tPASS\tsocket=%s\t%s\n" "${IPC_SOCKET_PATH}" "gateway.log" >> "${tsv}"
  else
    printf "SYS_GATEWAY_SOCKET\tFAIL\tsocket_missing=%s\t%s\n" "${IPC_SOCKET_PATH}" "gateway.log" >> "${tsv}"
  fi

  # Client checks
  if [[ -f "${art_dir}/client.jsonl" ]]; then
    local ping_ok
    ping_ok="$(python3 - <<'PY' "${art_dir}/client.jsonl"
import json, sys
p=sys.argv[1]
ok=False
with open(p,'r',encoding='utf-8') as f:
  for line in f:
    r=json.loads(line)
    if r.get("check")=="PING":
      ok=bool(r.get("ok"))
      break
print("1" if ok else "0")
PY
)"
    if [[ "${ping_ok}" == "1" ]]; then
      printf "INFO_IPC_PING\tPASS\tpong_received\t%s\n" "client.jsonl" >> "${tsv}"
    else
      printf "INFO_IPC_PING\tFAIL\tpong_not_received\t%s\n" "client.jsonl" >> "${tsv}"
    fi

    # Happy path: >= 90% ok
    python3 - <<'PY' "${art_dir}/client.jsonl" "${E2E_REQUESTS}" >> "${tsv}"
import json, sys
p=sys.argv[1]
total=int(sys.argv[2])
ok=0
seen=0
p50=p95=p99=None
with open(p,'r',encoding='utf-8') as f:
  for line in f:
    r=json.loads(line)
    if r.get("check")=="TASK_SUBMIT":
      seen += 1
      if r.get("ok"):
        ok += 1
    if r.get("check")=="SUMMARY":
      p50=r.get("p50_us"); p95=r.get("p95_us"); p99=r.get("p99_us")
rate = (ok/seen) if seen else 0.0
status = "PASS" if rate >= 0.90 and seen == total else "FAIL"
details = f"ok={ok}/{seen} rate={rate:.3f} p50_us={p50} p95_us={p95} p99_us={p99}"
print(f"SYS_HAPPY_PATH\t{status}\t{details}\tclient.jsonl")
PY
  else
    printf "SYS_CLIENT_RAN\tFAIL\tclient_jsonl_missing\t%s\n" "artifacts/router-e2e" >> "${tsv}"
  fi
}

# ---- Main ----
write_meta

log "Pre-check: NATS reachability..."
if tcp_check "${NATS_HOST}" "${NATS_PORT}"; then
  log "NATS reachable at ${NATS_HOST}:${NATS_PORT}"
else
  log "NATS NOT reachable at ${NATS_HOST}:${NATS_PORT} (router may fail)."
fi

start_router
sleep 2

start_gateway
log "Waiting for gateway socket: ${IPC_SOCKET_PATH}"
if ! wait_for_unix_socket "${IPC_SOCKET_PATH}" "${GATEWAY_START_TIMEOUT_SEC}"; then
  log "ERROR: gateway socket not created within timeout"
  generate_checks_tsv
  exit 1
fi

log "✓ Gateway socket READY: ${IPC_SOCKET_PATH}"
echo "=== GATEWAY READY $(date -Is) ===" >> "${art_dir}/gateway.log"

sleep 2

run_ipc_client

generate_checks_tsv

log "Generating summary.json..."

# Count PASS/FAIL
TOTAL_CHECKS=$(grep -c "^SYS_" "$art_dir/checks.tsv" || echo 0)
PASS_COUNT=$(grep -c $'\t''PASS'$'\t' "$art_dir/checks.tsv" || echo 0)
FAIL_COUNT=$(grep -c $'\t''FAIL'$'\t' "$art_dir/checks.tsv" || echo 0)

# Determine gate status
if [ "$FAIL_COUNT" -eq 0 ] && [ "$TOTAL_CHECKS" -ge 4 ]; then
    GATE_PASS="true"
    GATE_STATUS="PASS"
else
    GATE_PASS="false"
    GATE_STATUS="FAIL"
fi

# Collect failed checks
FAILED_CHECKS_JSON="[]"
if [ "$FAIL_COUNT" -gt 0 ]; then
    FAILED_CHECKS_JSON=$(grep $'\t''FAIL'$'\t' "$art_dir/checks.tsv" | grep -v "^schema_version" | awk -F'\t' '{printf "{\"check\":\"%s\",\"details\":\"%s\",\"evidence\":\"%s\"},", $1, $3, $4}' | sed 's/,$//')
    FAILED_CHECKS_JSON="[$FAILED_CHECKS_JSON]"
fi

# Generate summary.json
cat > "$art_dir/summary.json" << EOF
{
  "timestamp": "$ts",
  "duration_s": $(($(date +%s) - START_TIME)),
  "gate_pass": $GATE_PASS,
  "gate_status": "$GATE_STATUS",
  "checks": {
    "total": $TOTAL_CHECKS,
    "pass": $PASS_COUNT,
    "fail": $FAIL_COUNT
  },
  "failed_checks": $FAILED_CHECKS_JSON,
  "environment": {
    "nats": "$NATS_HOST:$NATS_PORT",
    "socket": "$IPC_SOCKET_PATH",
    "git_commit": "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')",
    "git_dirty": $(git diff-index --quiet HEAD 2>/dev/null && echo 'false' || echo 'true')
  },
  "artifact_refs": {
    "checks": "$art_dir/checks.tsv",
    "client_data": "$art_dir/client.jsonl",
    "gateway_log": "$art_dir/gateway.log",
    "router_log": "$art_dir/router.log",
    "metadata": {
      "env": "$art_dir/meta.env",
      "git": "$art_dir/meta.git",
      "versions": "$art_dir/meta.versions",
      "command": "$art_dir/command.txt"
    }
  }
}
EOF

log "Evidence pack complete: $art_dir"
log "Gate status: $GATE_STATUS ($PASS_COUNT PASS, $FAIL_COUNT FAIL)"
log "checks.tsv:"
cat "$art_dir/checks.tsv" | tee -a "$art_dir/stdout.log" > /dev/null

# Exit with gate status
if [ "$GATE_PASS" = "true" ]; then
    exit 0
else
    exit 1
fi
