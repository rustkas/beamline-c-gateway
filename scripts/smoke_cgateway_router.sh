#!/usr/bin/env bash
set -euo pipefail

# smoke_cgateway_router.sh
#
# Orchestrator (ADR-005 compliant by default):
#   start NATS -> start Router -> start c-gateway -> curl -> collect artifacts -> stop (always via trap)
#
# Modes:
#   Template mode (default): allows stub/dummy responses (ALLOW_DUMMY_RESPONSE=1)
#   Strict mode: requires non-dummy response (ALLOW_DUMMY_RESPONSE=0) + real NATS integration
#
# Determinism:
#   - bounded waits/timeouts
#   - timestamped artifacts in _artifacts/
#   - trap cleanup (SIGTERM -> SIGKILL fallback)
#
# Overrides:
#   - ROUTER_DIR, CGW_DIR
#   - ROUTER_START_CMD, CGW_START_CMD
#   - NATS_HOST/NATS_PORT/NATS_MON_PORT
#   - CGW_HOST/CGW_PORT, CGW_DECIDE_PATH, CGW_HEALTH_PATH
#   - STARTUP_WAIT_SECONDS, STOP_WAIT_SECONDS, CURL_TIMEOUT_SECONDS, CURL_RETRIES

log() { printf '[%s] %s\n' "$(date -u +'%Y-%m-%dT%H:%M:%SZ')" "$*" >&2; }
die() { log "ERROR: $*"; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cgw_root="$(cd "${script_dir}/.." && pwd)"

ALLOW_DUMMY_RESPONSE="${ALLOW_DUMMY_RESPONSE:-1}"

ARTIFACTS_DIR="${ARTIFACTS_DIR:-${cgw_root}/_artifacts}"
TS="$(date -u +'%Y%m%d_%H%M%S')"
RUN_DIR="${ARTIFACTS_DIR}/smoke_cgw_router_${TS}"

NATS_HOST="${NATS_HOST:-127.0.0.1}"
NATS_PORT="${NATS_PORT:-4222}"
NATS_MON_PORT="${NATS_MON_PORT:-8222}"

CGW_HOST="${CGW_HOST:-127.0.0.1}"
CGW_PORT="${CGW_PORT:-8080}"
CGW_DECIDE_PATH="${CGW_DECIDE_PATH:-/api/v1/routes/decide}"
CGW_HEALTH_PATH="${CGW_HEALTH_PATH:-/_health}"

CGW_BASE_URL="${CGW_BASE_URL:-http://${CGW_HOST}:${CGW_PORT}}"
CGW_DECIDE_URL="${CGW_DECIDE_URL:-${CGW_BASE_URL}${CGW_DECIDE_PATH}}"
CGW_HEALTH_URL="${CGW_HEALTH_URL:-${CGW_BASE_URL}${CGW_HEALTH_PATH}}"

STARTUP_WAIT_SECONDS="${STARTUP_WAIT_SECONDS:-20}"
STOP_WAIT_SECONDS="${STOP_WAIT_SECONDS:-6}"
CURL_TIMEOUT_SECONDS="${CURL_TIMEOUT_SECONDS:-5}"
CURL_RETRIES="${CURL_RETRIES:-10}"
CURL_RETRY_SLEEP_MS="${CURL_RETRY_SLEEP_MS:-200}"

ROUTER_DIR="${ROUTER_DIR:-}"
CGW_DIR="${CGW_DIR:-${cgw_root}}"

ROUTER_START_CMD="${ROUTER_START_CMD:-}"
CGW_START_CMD="${CGW_START_CMD:-}"

mkdir -p "${RUN_DIR}"

# ----------------- utils -----------------
python_json_load() {
  python3 - <<'PY'
import json,sys
json.load(sys.stdin)
print("ok")
PY
}

python_json_get() {
  # args: <json_file> <key>
  python3 - "$@" <<'PY'
import json,sys
p=sys.argv[1]
k=sys.argv[2]
with open(p,'r',encoding='utf-8') as f:
  obj=json.load(f)
v=obj.get(k,None)
if v is None:
  sys.exit(2)
if isinstance(v,(dict,list)):
  print(json.dumps(v,ensure_ascii=False))
else:
  print(v)
PY
}

sleep_ms() {
  python3 - <<PY
import time
time.sleep(${1}/1000.0)
PY
}

wait_http_200() {
  # $1=url, $2=label
  local url="$1"
  local label="$2"
  local deadline=$(( $(date +%s) + STARTUP_WAIT_SECONDS ))
  while true; do
    if curl -fsS --max-time "${CURL_TIMEOUT_SECONDS}" "${url}" >/dev/null 2>&1; then
      log "${label}: ready (${url})"
      return 0
    fi
    if [ "$(date +%s)" -ge "${deadline}" ]; then
      log "${label}: NOT ready after ${STARTUP_WAIT_SECONDS}s (${url})"
      return 1
    fi
    sleep 0.2
  done
}

kill_tree() {
  # $1=pid, $2=label
  local pid="$1"
  local label="$2"

  if [ -z "${pid}" ] || ! kill -0 "${pid}" >/dev/null 2>&1; then
    return 0
  fi

  log "Stopping ${label} (pid=${pid})..."
  kill -TERM "${pid}" >/dev/null 2>&1 || true

  local deadline=$(( $(date +%s) + STOP_WAIT_SECONDS ))
  while kill -0 "${pid}" >/dev/null 2>&1; do
    if [ "$(date +%s)" -ge "${deadline}" ]; then
      break
    fi
    sleep 0.2
  done

  if kill -0 "${pid}" >/dev/null 2>&1; then
    log "${label}: still alive -> SIGKILL"
    kill -KILL "${pid}" >/dev/null 2>&1 || true
  fi
}

discover_router_dir() {
  if [ -n "${ROUTER_DIR}" ] && [ -d "${ROUTER_DIR}" ]; then
    printf '%s\n' "${ROUTER_DIR}"
    return 0
  fi

  # Heuristic: sibling repo layout: ../otp/router or ../apps/otp/router
  local cand=""
  cand="$(find "${cgw_root}/.." -maxdepth 5 -type d -path '*/apps/otp/router' -print -quit 2>/dev/null || true)"
  if [ -n "${cand}" ] && [ -d "${cand}" ]; then
    printf '%s\n' "${cand}"
    return 0
  fi
  return 1
}

discover_cgw_start_cmd() {
  if [ -n "${CGW_START_CMD}" ]; then
    printf '%s\n' "${CGW_START_CMD}"
    return 0
  fi

  # Prefer dedicated script if exists
  if [ -x "${CGW_DIR}/scripts/run.sh" ]; then
    printf 'cd %q && %q --host %q --port %q\n' "${CGW_DIR}" "${CGW_DIR}/scripts/run.sh" "${CGW_HOST}" "${CGW_PORT}"
    return 0
  fi
  if [ -x "${CGW_DIR}/scripts/start.sh" ]; then
    printf 'cd %q && %q\n' "${CGW_DIR}" "${CGW_DIR}/scripts/start.sh"
    return 0
  fi

  # Try to find an executable named like gateway/c-gateway in build dirs
  local exe=""
  exe="$(find "${CGW_DIR}" -maxdepth 4 -type f -perm -111 \
    \( -name 'c-gateway' -o -name 'cgateway' -o -name 'gateway' -o -name '*gateway*' \) \
    2>/dev/null | head -n 1 || true)"

  if [ -n "${exe}" ]; then
    # Unknown flags; best-effort: run without args
    # Users can override CGW_START_CMD in CI for deterministic flags.
    printf 'cd %q && %q\n' "${CGW_DIR}" "${exe}"
    return 0
  fi

  return 1
}

discover_router_start_cmd() {
  if [ -n "${ROUTER_START_CMD}" ]; then
    printf '%s\n' "${ROUTER_START_CMD}"
    return 0
  fi

  # Prefer router scripts if present
  if [ -n "${router_dir}" ] && [ -x "${router_dir}/scripts/router_start.sh" ]; then
    printf 'cd %q && %q\n' "${router_dir}" "${router_dir}/scripts/router_start.sh"
    return 0
  fi

  # Try OTP release layout (best-effort)
  if [ -n "${router_dir}" ]; then
    local rel_bin=""
    rel_bin="$(find "${router_dir}" -maxdepth 6 -type f -perm -111 -path '*/rel/*/bin/*' \
      2>/dev/null | grep -E '/bin/beamline_router$|/bin/router$' | head -n 1 || true)"
    if [ -n "${rel_bin}" ]; then
      # foreground keeps it attached; we background it with our own &
      printf 'cd %q && %q foreground\n' "${router_dir}" "${rel_bin}"
      return 0
    fi
  fi

  return 1
}

# ----------------- trap / cleanup -----------------
nats_pid=""
router_pid=""
cgw_pid=""
router_dir=""

cleanup() {
  local rc=$?
  set +e

  log "Capturing env snapshot..."
  {
    echo "timestamp_utc=${TS}"
    echo "run_dir=${RUN_DIR}"
    echo "cgw_root=${cgw_root}"
    echo "router_dir=${router_dir}"
    echo "ALLOW_DUMMY_RESPONSE=${ALLOW_DUMMY_RESPONSE}"
    echo "NATS=${NATS_HOST}:${NATS_PORT} (mon ${NATS_MON_PORT})"
    echo "CGW_BASE_URL=${CGW_BASE_URL}"
    echo "CGW_DECIDE_URL=${CGW_DECIDE_URL}"
    echo "CGW_HEALTH_URL=${CGW_HEALTH_URL}"
    echo "uname=$(uname -a)"
    echo "git_cgateway=$(cd "${cgw_root}" && (git rev-parse --short HEAD 2>/dev/null || echo "nogit"))"
    if [ -n "${router_dir}" ]; then
      echo "git_router=$(cd "${router_dir}" && (git rev-parse --short HEAD 2>/dev/null || echo "nogit"))"
    fi
    if have nats-server; then echo "nats_server=$(nats-server -v 2>/dev/null || true)"; fi
    if have curl; then echo "curl=$(curl --version 2>/dev/null | head -n 1 || true)"; fi
    if have python3; then echo "python3=$(python3 --version 2>/dev/null || true)"; fi
  } > "${RUN_DIR}/env.txt" 2>/dev/null || true

  log "Stopping services..."
  kill_tree "${cgw_pid}" "c-gateway"
  kill_tree "${router_pid}" "router"
  kill_tree "${nats_pid}" "nats-server"

  # If we used router nats scripts, try stop for extra safety (best-effort)
  if [ -n "${router_dir}" ] && [ -x "${router_dir}/scripts/nats_stop.sh" ]; then
    "${router_dir}/scripts/nats_stop.sh" > "${RUN_DIR}/nats_stop.log" 2>&1 || true
  fi

  log "Artifacts saved: ${RUN_DIR}"
  exit "${rc}"
}
trap cleanup EXIT INT TERM

banner() {
  log "============================================================"
  log " smoke_cgateway_router: mode=$( [ "${ALLOW_DUMMY_RESPONSE}" = "1" ] && echo TEMPLATE || echo STRICT )"
  if [ "${ALLOW_DUMMY_RESPONSE}" = "1" ]; then
    log " TEMPLATE MODE: dummy/stub responses are accepted (ADR-005 safe)."
  else
    log " STRICT MODE: dummy/stub responses will FAIL (requires real integration)."
  fi
  log "============================================================"
}

# ----------------- start NATS -----------------
start_nats() {
  log "Starting NATS..."
  if [ -n "${router_dir}" ] && [ -x "${router_dir}/scripts/nats_start.sh" ]; then
    "${router_dir}/scripts/nats_start.sh" > "${RUN_DIR}/nats_start.log" 2>&1 || {
      log "router nats_start.sh failed; see ${RUN_DIR}/nats_start.log"
      return 1
    }
    wait_http_200 "http://${NATS_HOST}:${NATS_MON_PORT}/healthz" "nats-monitor"
    return 0
  fi

  if ! have nats-server; then
    die "nats-server not found and router/scripts/nats_start.sh not available."
  fi

  local store_dir="${RUN_DIR}/nats_store"
  mkdir -p "${store_dir}"

  nats-server -js -a "${NATS_HOST}" -p "${NATS_PORT}" -m "${NATS_MON_PORT}" -sd "${store_dir}" \
    > "${RUN_DIR}/nats.log" 2>&1 &
  nats_pid="$!"
  echo "${nats_pid}" > "${RUN_DIR}/nats.pid"

  wait_http_200 "http://${NATS_HOST}:${NATS_MON_PORT}/healthz" "nats-monitor"
}

# ----------------- start Router -----------------
start_router() {
  local cmd=""
  cmd="$(discover_router_start_cmd || true)"
  if [ -z "${cmd}" ]; then
    die "ROUTER_START_CMD not set and auto-discovery failed. Set ROUTER_START_CMD for deterministic CI."
  fi

  log "Starting Router..."
  # shellcheck disable=SC2016
  bash -lc "${cmd}" > "${RUN_DIR}/router.log" 2>&1 &
  router_pid="$!"
  echo "${router_pid}" > "${RUN_DIR}/router.pid"

  # No universal health endpoint known; give a short warmup
  sleep 1.0
  log "Router started (pid=${router_pid})."
}

# ----------------- start c-gateway -----------------
start_cgateway() {
  local cmd=""
  cmd="$(discover_cgw_start_cmd || true)"
  if [ -z "${cmd}" ]; then
    die "CGW_START_CMD not set and auto-discovery failed. Set CGW_START_CMD for deterministic CI."
  fi

  log "Starting c-gateway..."
  # Provide NATS env for future strict mode; harmless in stub mode
  export NATS_URL="nats://${NATS_HOST}:${NATS_PORT}"
  export ROUTER_NATS_SUBJECT="${ROUTER_NATS_SUBJECT:-beamline.router.v1.decide}"

  # shellcheck disable=SC2016
  bash -lc "${cmd}" > "${RUN_DIR}/c_gateway.log" 2>&1 &
  cgw_pid="$!"
  echo "${cgw_pid}" > "${RUN_DIR}/c_gateway.pid"

  wait_http_200 "${CGW_HEALTH_URL}" "c-gateway"
}

# ----------------- curl + validate -----------------
run_curl() {
  have curl || die "curl not found"
  have python3 || die "python3 not found"

  log "Running HTTP decide: ${CGW_DECIDE_URL}"

  local req_file="${RUN_DIR}/request.json"
  local headers_file="${RUN_DIR}/response.headers"
  local body_file="${RUN_DIR}/response.body"
  local http_code="000"

  cat > "${req_file}" <<JSON
{
  "message_id": "smoke-${TS}",
  "tenant_id": "tenant-smoke",
  "policy_id": "demo-policy",
  "input": { "text": "hello" }
}
JSON

  local attempt=1
  while [ "${attempt}" -le "${CURL_RETRIES}" ]; do
    rm -f "${headers_file}" "${body_file}" || true

    http_code="$(
      curl -sS --max-time "${CURL_TIMEOUT_SECONDS}" \
        -D "${headers_file}" \
        -o "${body_file}" \
        -w '%{http_code}' \
        -X POST "${CGW_DECIDE_URL}" \
        -H 'Content-Type: application/json' \
        -H 'X-Request-ID: smoke-request' \
        -H 'X-Trace-ID: smoke-trace' \
        --data-binary @"${req_file}" \
        || echo "000"
    )"

    echo "${http_code}" > "${RUN_DIR}/http_code.txt"

    if [ "${http_code}" = "200" ]; then
      if python_json_load < "${body_file}" >/dev/null 2>&1; then
        log "HTTP 200 + valid JSON"

        # dummy detection: message_id == "dummy" OR contains "dummy" marker in message_id
        local msg_id=""
        msg_id="$(python_json_get "${body_file}" "message_id" 2>/dev/null || true)"
        echo "${msg_id}" > "${RUN_DIR}/message_id.txt" 2>/dev/null || true

        if [ "${ALLOW_DUMMY_RESPONSE}" = "1" ]; then
          log "Template mode: accepting response (message_id=${msg_id:-<none>})"
          return 0
        fi

        if [ "${msg_id}" = "dummy" ] || printf '%s' "${msg_id}" | grep -qi 'dummy'; then
          log "STRICT MODE FAIL: dummy response detected (message_id=${msg_id})"
          return 1
        fi

        log "Strict mode: non-dummy response OK (message_id=${msg_id:-<none>})"
        return 0
      fi

      log "HTTP 200 but invalid JSON (attempt ${attempt}/${CURL_RETRIES})"
    else
      log "HTTP ${http_code} (attempt ${attempt}/${CURL_RETRIES})"
    fi

    attempt=$((attempt + 1))
    sleep_ms "${CURL_RETRY_SLEEP_MS}"
  done

  die "curl failed after ${CURL_RETRIES} attempts (last http_code=${http_code})"
}

snapshot_nats() {
  curl -fsS --max-time "${CURL_TIMEOUT_SECONDS}" "http://${NATS_HOST}:${NATS_MON_PORT}/varz" > "${RUN_DIR}/nats_varz.json" 2>/dev/null || true
  curl -fsS --max-time "${CURL_TIMEOUT_SECONDS}" "http://${NATS_HOST}:${NATS_MON_PORT}/connz" > "${RUN_DIR}/nats_connz.json" 2>/dev/null || true
}

main() {
  banner

  router_dir="$(discover_router_dir || true)"
  if [ -n "${router_dir}" ]; then
    log "Detected router_dir: ${router_dir}"
  else
    log "router_dir not detected (set ROUTER_DIR if needed)."
  fi

  start_nats
  start_router
  start_cgateway
  run_curl
  snapshot_nats

  log "SUCCESS: smoke_cgateway_router completed."
}

main "$@"
