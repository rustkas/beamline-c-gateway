#!/usr/bin/env bash
# run_router_e2e_with_evidence.sh - Facts-only Router E2E with TSV scoring
# User's FINAL specification

set -euo pipefail

ts="$(date +%Y%m%d_%H%M%S)"
root="${ARTIFACTS_DIR:-artifacts/router-e2e}/${ts}"
logs="${root}/logs"
mkdir -p "${logs}"

# === CONFIG ===
socket_path="${IPC_SOCKET_PATH:-/tmp/beamline-gateway.sock}"
nats_url="${NATS_URL:-nats://127.0.0.1:4222}"

router_cmd="${ROUTER_CMD:-}"
gateway_cmd="${GATEWAY_CMD:-}"
client_cmd="${CLIENT_CMD:-}"

if [[ -z "${router_cmd}" || -z "${gateway_cmd}" || -z "${client_cmd}" ]]; then
  echo "Missing ROUTER_CMD / GATEWAY_CMD / CLIENT_CMD" >&2
  exit 2
fi

# === SNAPSHOT ===
{
  echo "ts=${ts}"
  echo "socket_path=${socket_path}"
  echo "nats_url=${nats_url}"
  echo "git_rev=$(git rev-parse HEAD 2>/dev/null || echo unknown)"
  echo "uname=$(uname -a)"
  echo "whoami=$(whoami)"
} > "${root}/environment.txt"

{
  echo "ROUTER_CMD=${router_cmd}"
  echo "GATEWAY_CMD=${gateway_cmd}"
  echo "CLIENT_CMD=${client_cmd}"
} > "${root}/command.txt"

# === PROCESS CONTROL ===
router_pid=""
gateway_pid=""

cleanup() {
  set +e
  if [[ -n "${gateway_pid}" ]]; then kill "${gateway_pid}" 2>/dev/null || true; fi
  if [[ -n "${router_pid}" ]]; then kill "${router_pid}" 2>/dev/null || true; fi
}
trap cleanup EXIT

# Start router
bash -lc "${router_cmd}" > "${logs}/router.log" 2>&1 &
router_pid="$!"

# Start gateway
bash -lc "${gateway_cmd}" > "${logs}/gateway.log" 2>&1 &
gateway_pid="$!"

: > "${logs}/nats.log"

# === CHECKS TSV ===
checks="${root}/checks.tsv"
: > "${checks}"

append_check() {
  local id="$1" weight="$2" pass="$3" note="$4"
  printf "%s\t%s\t%s\t%s\n" "${id}" "${weight}" "${pass}" "${note}" >> "${checks}"
}

# === CORE CHECKS ===
if [[ -S "${socket_path}" ]]; then
  append_check "CORE_SOCKET_EXISTS" "10" "1" "ok"
else
  append_check "CORE_SOCKET_EXISTS" "10" "0" "missing_socket"
fi

set +e
bash -lc "${client_cmd} --mode ping --socket '${socket_path}'" >> "${logs}/client.log" 2>&1
rc=$?
set -e
if [[ "${rc}" -eq 0 ]]; then
  append_check "CORE_PROTOCOL_PING" "20" "1" "ok"
else
  append_check "CORE_PROTOCOL_PING" "20" "0" "client_rc=${rc}"
fi

# === SYSTEM SCENARIOS ===
run_scenario() {
  local id="$1" weight="$2" cmd="$3"
  set +e
  bash -lc "${cmd}" >> "${logs}/client.log" 2>&1
  local rc=$?
  set -e
  if [[ "${rc}" -eq 0 ]]; then
    append_check "${id}" "${weight}" "1" "ok"
  else
    append_check "${id}" "${weight}" "0" "rc=${rc}"
  fi
}

run_scenario "SYS_HAPPY_PATH_1K"       "10" "tests/e2e_router_happy_path.sh --nats '${nats_url}'"
run_scenario "SYS_SUBJECTS_HEADERS"    "20" "tests/e2e_router_subjects_headers.sh --nats '${nats_url}'"
run_scenario "SYS_ERRORS_4XX_5XX"      "20" "tests/e2e_router_errors.sh --nats '${nats_url}'"
run_scenario "SYS_TIMEOUTS_LATE_REPLY" "20" "tests/e2e_router_timeouts.sh --nats '${nats_url}'"
run_scenario "SYS_RECONNECT_STORM"     "20" "tests/e2e_router_reconnect_storm.sh --nats '${nats_url}'"

# === SCORE ===
overall_pct="$(awk -F'\t' '{w+=$2; p+=$2*$3} END{ if (w==0) print 0; else printf "%.2f", (p/w)*100 }' "${checks}")"
core_pct="$(awk -F'\t' '$1 ~ /^CORE_/ {w+=$2; p+=$2*$3} END{ if (w==0) print 0; else printf "%.2f", (p/w)*100 }' "${checks}")"
system_pct="$(awk -F'\t' '$1 ~ /^SYS_/ {w+=$2; p+=$2*$3} END{ if (w==0) print 0; else printf "%.2f", (p/w)*100 }' "${checks}")"

printf "%s\t%s\t%s\n" "${core_pct}" "${system_pct}" "${overall_pct}" > "${root}/readiness.tsv"

echo "0" > "${root}/exit_code.txt"
echo "artifacts_dir=${root}"
echo "core_pct=${core_pct} system_pct=${system_pct} overall_pct=${overall_pct}"
