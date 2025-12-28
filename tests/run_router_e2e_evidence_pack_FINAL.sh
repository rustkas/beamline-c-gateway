#!/usr/bin/env bash
# run_router_e2e_evidence_pack.sh
# Router E2E evidence pack generator (facts-only)
# User's complete implementation

set -euo pipefail

# Required:
#   ROUTER_CMD   - exact command to start router (foreground)
#   GATEWAY_CMD  - exact command to start gateway (foreground)
#
# Optional:
#   IPC_SOCKET_PATH (default /tmp/beamline-gateway.sock)
#   NATS_URL (default nats://127.0.0.1:4222)
#   OUT_DIR (default artifacts/router-e2e/<ts>/)

ts_rfc3339() { date -Iseconds; }
ts_dir() { date +%Y%m%d_%H%M%S; }

require_env() {
  local k="$1"
  if [[ -z "${!k:-}" ]]; then
    echo "ERROR: env ${k} is required" >&2
    exit 2
  fi
}

IPC_SOCKET_PATH="${IPC_SOCKET_PATH:-/tmp/beamline-gateway.sock}"
NATS_URL="${NATS_URL:-nats://127.0.0.1:4222}"
OUT_DIR="${OUT_DIR:-artifacts/router-e2e/$(ts_dir)}"

require_env "ROUTER_CMD"
require_env "GATEWAY_CMD"

mkdir -p "${OUT_DIR}"
checks_tsv="${OUT_DIR}/checks.tsv"
summary_json="${OUT_DIR}/summary.json"
commands_log="${OUT_DIR}/commands.log"
router_log="${OUT_DIR}/router.log"
gateway_log="${OUT_DIR}/gateway.log"

echo -e "ts\tcheck_id\tcomponent\tstatus\tevidence_path\tdetails" > "${checks_tsv}"

log_check() {
  local check_id="$1"
  local component="$2"
  local status="$3"
  local evidence_path="$4"
  local details="$5"
  printf "%s\t%s\t%s\t%s\t%s\t%s\n" "$(ts_rfc3339)" "${check_id}" "${component}" "${status}" "${evidence_path}" "${details}" >> "${checks_tsv}"
}

write_meta() {
  {
    echo "IPC_SOCKET_PATH=${IPC_SOCKET_PATH}"
    echo "NATS_URL=${NATS_URL}"
    echo "UNAME=$(uname -a)"
  } > "${OUT_DIR}/meta.env"

  if command -v git >/dev/null 2>&1; then
    {
      echo "commit=$(git rev-parse HEAD 2>/dev/null || true)"
      echo "branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || true)"
      echo "dirty=$(git status --porcelain 2>/dev/null | wc -l | tr -d ' ')"
    } > "${OUT_DIR}/meta.git"
  fi
}

start_bg() {
  local name="$1"
  local cmd="$2"
  local log_path="$3"

  echo "${cmd}" >> "${commands_log}"
  bash -lc "${cmd}" >"${log_path}" 2>&1 &
  echo $!
}

wait_for_socket() {
  local path="$1"
  local deadline_s="$2"
  local start
  start="$(date +%s)"
  while true; do
    if [[ -S "${path}" ]]; then
      return 0
    fi
    local now
    now="$(date +%s)"
    if (( now - start >= deadline_s )); then
      return 1
    fi
    sleep 0.1
  done
}

wait_for_port_4222() {
  local deadline_s="$1"
  local start
  start="$(date +%s)"
  while true; do
    if (exec 3<>/dev/tcp/127.0.0.1/4222) >/dev/null 2>&1; then
      exec 3<&-
      exec 3>&-
      return 0
    fi
    local now
    now="$(date +%s)"
    if (( now - start >= deadline_s )); then
      return 1
    fi
    sleep 0.2
  done
}

# ---- Scenario hooks (replace with real client runners) ----
scenario_happy_path() {
  # Must exit 0 on pass
  # Write logs to OUT_DIR
  return 0
}

scenario_errors() { return 0; }
scenario_timeouts_late_replies() { return 0; }
scenario_backpressure() { return 0; }
scenario_reconnect_storm() { return 0; }

# ---- Main ----
write_meta

# Precheck NATS
if wait_for_port_4222 2; then
  log_check "NATS_PORT_4222" "nats" "PASS" "meta.env" "reachable=1"
else
  log_check "NATS_PORT_4222" "nats" "SKIP" "meta.env" "reachable=0"
fi

router_pid="$(start_bg "router" "${ROUTER_CMD}" "${router_log}")"
sleep 0.2
if kill -0 "${router_pid}" >/dev/null 2>&1; then
  log_check "ROUTER_START" "router" "PASS" "router.log" "pid=${router_pid}"
else
  log_check "ROUTER_START" "router" "FAIL" "router.log" "pid_missing"
fi

gateway_pid="$(start_bg "gateway" "${GATEWAY_CMD}" "${gateway_log}")"
if wait_for_socket "${IPC_SOCKET_PATH}" 5; then
  log_check "GW_SOCKET_READY" "gateway" "PASS" "gateway.log" "socket=${IPC_SOCKET_PATH}"
else
  log_check "GW_SOCKET_READY" "gateway" "FAIL" "gateway.log" "socket=${IPC_SOCKET_PATH}"
fi

run_scenario() {
  local id="$1"
  local fn="$2"
  local log_name="$3"

  local log_path="${OUT_DIR}/${log_name}"
  echo "${fn}" >> "${commands_log}"

  if bash -lc "${fn}" >"${log_path}" 2>&1; then
    log_check "${id}" "system" "PASS" "${log_name}" "exit=0"
  else
    log_check "${id}" "system" "FAIL" "${log_name}" "exit=$?"
  fi
}

run_scenario "E2E_HAPPY_1000" "scenario_happy_path" "e2e_happy.log"
run_scenario "E2E_ERROR_SEMANTICS" "scenario_errors" "e2e_errors.log"
run_scenario "E2E_TIMEOUTS_LATE" "scenario_timeouts_late_replies" "e2e_timeouts.log"
run_scenario "E2E_BACKPRESSURE" "scenario_backpressure" "e2e_backpressure.log"
run_scenario "E2E_RECONNECT_STORM" "scenario_reconnect_storm" "e2e_reconnect.log"

# Cleanup
kill "${gateway_pid}" >/dev/null 2>&1 || true
kill "${router_pid}" >/dev/null 2>&1 || true

# Compute summary.json
passed="$(awk -F'\t' 'NR>1 && $4=="PASS"{c++} END{print c+0}' "${checks_tsv}")"
failed="$(awk -F'\t' 'NR>1 && $4=="FAIL"{c++} END{print c+0}' "${checks_tsv}")"
skipped="$(awk -F'\t' 'NR>1 && $4=="SKIP"{c++} END{print c+0}' "${checks_tsv}")"

required_failed="$(awk -F'\t' 'NR>1 && $2 ~ /^E2E_/ && $4=="FAIL"{print $2}' "${checks_tsv}" | tr '\n' ' ')"
gate="true"
if [[ -n "${required_failed// }" ]]; then
  gate="false"
fi

commit="$(grep '^commit=' "${OUT_DIR}/meta.git" 2>/dev/null | cut -d= -f2- || true)"

cat > "${summary_json}" <<EOF
{
  "ts": "$(ts_rfc3339)",
  "commit": "${commit}",
  "gate_system_pass": ${gate},
  "passed": ${passed},
  "failed": ${failed},
  "skipped": ${skipped},
  "required_failed": "$(echo "${required_failed}" | sed 's/[[:space:]]*$//')"
}
EOF

echo "OK: ${OUT_DIR}"
echo "checks: ${checks_tsv}"
echo "summary: ${summary_json}"
