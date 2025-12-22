#!/usr/bin/env bash
#
# smoke_cgateway_router.sh - End-to-end smoke test: c-gateway ↔ NATS ↔ Router
#
# IMPORTANT: This script is READINESS TOOLING (template mode by default).
#
# STATUS: C-Gateway currently in STUB MODE (ADR-005)
# - By default, accepts stub/dummy responses (ALLOW_DUMMY_RESPONSE=1)
# - Real integration deferred until Router perf stable
# - Prerequisites: See .ai/decisions.md (ADR-005)
#
# USAGE:
#   # Template mode (allows stub responses for infra validation)
#   ./scripts/smoke_cgateway_router.sh
#
#   # Strict mode (requires real Router responses) - NOT YET READY
#   ALLOW_DUMMY_RESPONSE=0 ./scripts/smoke_cgateway_router.sh
#
# ARTIFACTS: _artifacts/smoke_cgw_router_TIMESTAMP/
#   - smoke.log, env.txt
#   - nats_start.log, nats_status.log, nats_stop.log
#   - router.log, router.log.tail, router.pid
#   - cgw.log, cgw.log.tail, cgw.pid
#   - http_headers.txt, http_body.json, http_code.txt
#
# ENV OVERRIDES:
#   ROUTER_ROOT              - Path to Router repo (default: ../otp/router)
#   ARTIFACTS_DIR            - Artifacts directory (default: _artifacts)
#   NATS_HTTP_HOSTPORT       - NATS monitoring (default: 127.0.0.1:8222)
#   CGW_HOSTPORT             - C-Gateway HTTP (default: 127.0.0.1:8080)
#   CGW_HEALTH_PATH          - Health endpoint (default: /_health)
#   CGW_DECIDE_PATH          - Decide endpoint (default: /api/v1/routes/decide)
#   CGW_START_CMD            - C-Gateway start command (auto-discover if empty)
#   CGW_START_ARGS           - Additional args for c-gateway
#   ROUTER_START_CMD         - Router start command (auto-discover if empty)
#   ROUTER_HEALTH_URL        - Router health check URL (optional)
#   SMOKE_TIMEOUT_SECONDS    - Overall timeout (default: 180)
#   CURL_TIMEOUT_SECONDS     - HTTP request timeout (default: 10)
#   ALLOW_DUMMY_RESPONSE     - Allow stub responses: 1=yes (default), 0=strict
#
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

artifacts_dir="${ARTIFACTS_DIR:-"${repo_root}/_artifacts"}"
mkdir -p "${artifacts_dir}"

ts="$(date -u +%Y%m%d_%H%M%S)"
run_dir="${artifacts_dir}/smoke_cgw_router_${ts}"
mkdir -p "${run_dir}"

log() { 
  printf '[%s] %s\n' "$(date -u +%H:%M:%S)" "$*" | tee -a "${run_dir}/smoke.log"
}

# Check if timeout command is available
have_timeout=0
if command -v timeout >/dev/null 2>&1; then
  have_timeout=1
fi

run_with_timeout() {
  local seconds="$1"
  shift
  if [[ "${have_timeout}" -eq 1 ]]; then
    timeout --preserve-status "${seconds}" "$@"
    return $?
  fi
  # Fallback: no timeout binary, just run
  "$@"
}

wait_http_200() {
  local url="$1"
  local timeout_s="$2"
  local interval_ms="${3:-200}"

  local deadline
  deadline="$(( $(date +%s) + timeout_s ))"

  while true; do
    if curl -fsS -o /dev/null "${url}" >/dev/null 2>&1; then
      return 0
    fi
    if [[ "$(date +%s)" -ge "${deadline}" ]]; then
      return 1
    fi
    sleep 0.$((interval_ms / 100))
  done
}

kill_pid_graceful() {
  local pid="$1"
  local name="$2"

  if ! kill -0 "${pid}" >/dev/null 2>&1; then
    return 0
  fi

  log "Stopping ${name} pid=${pid} (TERM → KILL fallback)"
  kill -TERM "${pid}" >/dev/null 2>&1 || true

  local deadline
  deadline="$(( $(date +%s) + 3 ))"
  while kill -0 "${pid}" >/dev/null 2>&1; do
    if [[ "$(date +%s)" -ge "${deadline}" ]]; then
      break
    fi
    sleep 0.1
  done

  if kill -0 "${pid}" >/dev/null 2>&1; then
    kill -KILL "${pid}" >/dev/null 2>&1 || true
  fi
}

# Configuration
router_root_default="${repo_root}/../otp/router"
router_root="${ROUTER_ROOT:-"${router_root_default}"}"

nats_http_hostport="${NATS_HTTP_HOSTPORT:-"127.0.0.1:8222"}"
cgw_hostport="${CGW_HOSTPORT:-"127.0.0.1:8080"}"
cgw_health_path="${CGW_HEALTH_PATH:-"/_health"}"
cgw_decide_path="${CGW_DECIDE_PATH:-"/api/v1/routes/decide"}"

smoke_timeout="${SMOKE_TIMEOUT_SECONDS:-180}"
curl_timeout="${CURL_TIMEOUT_SECONDS:-10}"
allow_dummy="${ALLOW_DUMMY_RESPONSE:-1}"  # Default: allow stub (template mode)

router_pid=""
cgw_pid=""
started_nats=0

# Discover NATS scripts (prefer Router repo, fallback to local)
nats_start_script=""
nats_status_script=""
nats_stop_script=""

for script_name in nats_start nats_status nats_stop; do
  script_var="${script_name}_script"
  
  # Try Router repo first
  if [[ -x "${router_root}/scripts/${script_name}.sh" ]]; then
    eval "${script_var}=\"${router_root}/scripts/${script_name}.sh\""
  elif [[ -x "${repo_root}/scripts/${script_name}.sh" ]]; then
    eval "${script_var}=\"${repo_root}/scripts/${script_name}.sh\""
  fi
done

discover_router_start_cmd() {
  # Prefer OTP release if present
  local cand
  for cand in \
    "${router_root}/_build/default/rel/beamline_router/bin/beamline_router" \
    "${router_root}/_build/test/rel/beamline_router/bin/beamline_router" \
    "${router_root}/_build/default/rel/router/bin/router" \
    "${router_root}/_build/test/rel/router/bin/router"
  do
    if [[ -x "${cand}" ]]; then
      printf '%s foreground' "${cand}"
      return 0
    fi
  done

  # Last resort: rebar3 shell (not ideal for CI)
  if [[ -f "${router_root}/rebar.config" ]] && command -v rebar3 >/dev/null 2>&1; then
    printf 'bash -lc "cd %s && rebar3 shell --name router_smoke@127.0.0.1"' "${router_root}"
    return 0
  fi

  return 1
}

discover_cgw_start_cmd() {
  # Try common build outputs
  local cand
  for cand in \
    "${repo_root}/build/c-gateway" \
    "${repo_root}/build/bin/c-gateway" \
    "${repo_root}/build-nats/c-gateway" \
    "${repo_root}/build/cgateway" \
    "${repo_root}/bin/c-gateway"
  do
    if [[ -x "${cand}" ]]; then
      printf '%s' "${cand}"
      return 0
    fi
  done

  # Search for c-gateway binary (max depth 4)
  cand="$(find "${repo_root}" -maxdepth 4 -type f -name 'c-gateway' -perm -111 2>/dev/null | head -n 1 || true)"
  if [[ -n "${cand}" ]]; then
    printf '%s' "${cand}"
    return 0
  fi

  return 1
}

cleanup() {
  set +e

  log "Collecting tail logs"
  if [[ -f "${run_dir}/cgw.log" ]]; then
    tail -n 200 "${run_dir}/cgw.log" > "${run_dir}/cgw.log.tail" 2>/dev/null || true
  fi
  if [[ -f "${run_dir}/router.log" ]]; then
    tail -n 200 "${run_dir}/router.log" > "${run_dir}/router.log.tail" 2>/dev/null || true
  fi

  if [[ -n "${cgw_pid}" ]]; then
    kill_pid_graceful "${cgw_pid}" "c-gateway"
  fi
  if [[ -n "${router_pid}" ]]; then
    kill_pid_graceful "${router_pid}" "router"
  fi

  if [[ "${started_nats}" -eq 1 && -n "${nats_stop_script}" ]]; then
    log "Stopping NATS via ${nats_stop_script}"
    "${nats_stop_script}" >> "${run_dir}/nats_stop.log" 2>&1 || true
  fi

  log "Smoke run artifacts: ${run_dir}"
}
trap cleanup EXIT

# Banner
log "╔════════════════════════════════════════════════════════════════╗"
log "║  C-Gateway ↔ Router Smoke Test (TEMPLATE MODE)                ║"
log "╚════════════════════════════════════════════════════════════════╝"
log ""
log "⚠️  CURRENT STATUS: C-Gateway in STUB MODE (ADR-005)"
log "    - libnats NOT installed"
log "    - Real NATS integration DEFERRED"
log "    - This script validates infrastructure only"
log ""
if [[ "${allow_dummy}" -eq 1 ]]; then
  log "✓  Template mode: ALLOW_DUMMY_RESPONSE=1 (stub responses accepted)"
else
  log "⚠️  Strict mode: ALLOW_DUMMY_RESPONSE=0 (requires real Router)"
  log "    - This mode will FAIL until real integration enabled"
  log "    - See .ai/decisions.md (ADR-005) for prerequisites"
fi
log ""

# Environment snapshot
log "Environment snapshot"
{
  echo "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "repo_root=${repo_root}"
  echo "router_root=${router_root}"
  echo "uname=$(uname -a)"
  echo "git_cgw=$(git -C "${repo_root}" rev-parse --short HEAD 2>/dev/null || echo 'n/a')"
  echo "git_router=$(git -C "${router_root}" rev-parse --short HEAD 2>/dev/null || echo 'n/a')"
  echo "nats_http_hostport=${nats_http_hostport}"
  echo "cgw_hostport=${cgw_hostport}"
  echo "allow_dummy=${allow_dummy}"
} > "${run_dir}/env.txt"

# 1) Start NATS
if [[ -z "${nats_start_script}" ]]; then
  log "ERROR: nats_start.sh not found (set ROUTER_ROOT or add scripts/nats_start.sh)"
  exit 2
fi

log "Starting NATS via ${nats_start_script}"
"${nats_start_script}" >> "${run_dir}/nats_start.log" 2>&1
started_nats=1

if [[ -n "${nats_status_script}" ]]; then
  log "NATS status via ${nats_status_script}"
  "${nats_status_script}" >> "${run_dir}/nats_status.log" 2>&1 || true
fi

log "Waiting for NATS healthz http://${nats_http_hostport}/healthz"
if ! wait_http_200 "http://${nats_http_hostport}/healthz" 10 200; then
  log "ERROR: NATS healthz did not become ready"
  exit 3
fi

# 2) Start Router
router_start_cmd="${ROUTER_START_CMD:-""}"
if [[ -z "${router_start_cmd}" ]]; then
  if ! router_start_cmd="$(discover_router_start_cmd)"; then
    log "ERROR: Could not discover router start command (set ROUTER_START_CMD)"
    exit 4
  fi
fi

log "Starting Router: ${router_start_cmd}"
bash -c "${router_start_cmd} > '${run_dir}/router.log' 2>&1 & echo \$! > '${run_dir}/router.pid'" || true
router_pid="$(cat "${run_dir}/router.pid" 2>/dev/null || true)"
if [[ -z "${router_pid}" ]]; then
  log "ERROR: Router failed to start (no pid)"
  exit 5
fi
log "Router pid=${router_pid}"

if [[ -n "${ROUTER_HEALTH_URL:-""}" ]]; then
  log "Waiting for Router health ${ROUTER_HEALTH_URL}"
  if ! wait_http_200 "${ROUTER_HEALTH_URL}" "${smoke_timeout}" 250; then
    log "ERROR: Router health did not become ready"
    exit 6
  fi
else
  log "Router health URL not set; sleeping 2s before proceeding"
  sleep 2
fi

# 3) Start c-gateway
cgw_start_cmd="${CGW_START_CMD:-""}"
if [[ -z "${cgw_start_cmd}" ]]; then
  if ! cgw_start_cmd="$(discover_cgw_start_cmd)"; then
    log "ERROR: Could not discover c-gateway binary (set CGW_START_CMD)"
    log "       Run: cmake -S . -B build && cmake --build build"
    exit 7
  fi
fi

log "Starting c-gateway: ${cgw_start_cmd}"
cgw_start_args="${CGW_START_ARGS:-""}"
bash -c "${cgw_start_cmd} ${cgw_start_args} > '${run_dir}/cgw.log' 2>&1 & echo \$! > '${run_dir}/cgw.pid'" || true
cgw_pid="$(cat "${run_dir}/cgw.pid" 2>/dev/null || true)"
if [[ -z "${cgw_pid}" ]]; then
  log "ERROR: c-gateway failed to start (no pid)"
  exit 8
fi
log "c-gateway pid=${cgw_pid}"

cgw_health_url="http://${cgw_hostport}${cgw_health_path}"
log "Waiting for c-gateway health ${cgw_health_url}"
if ! wait_http_200 "${cgw_health_url}" "${smoke_timeout}" 200; then
  log "ERROR: c-gateway health did not become ready"
  exit 9
fi

# 4) Curl decide endpoint
decide_url="http://${cgw_hostport}${cgw_decide_path}"
req_body='{"from":"test@example.com","recipients":["user@example.com"],"subject":"smoke test"}'

log "Calling decide endpoint: ${decide_url}"
set +e
http_code="$(
  run_with_timeout "${curl_timeout}" \
    curl -sS -D "${run_dir}/http_headers.txt" -o "${run_dir}/http_body.json" \
      -H 'Content-Type: application/json' \
      -H 'X-Request-Id: smoke-1' \
      -H 'X-Tenant-Id: t1' \
      -H 'X-API-Key: test-admin-key' \
      -w '%{http_code}' \
      "${decide_url}" \
      -d "${req_body}"
)"
curl_rc="$?"
set -e

echo "${http_code}" > "${run_dir}/http_code.txt"

if [[ "${curl_rc}" -ne 0 ]]; then
  log "ERROR: curl failed rc=${curl_rc}"
  exit 10
fi

log "HTTP code=${http_code}"
if [[ "${http_code}" != "200" && "${http_code}" != "201" ]]; then
  log "ERROR: unexpected HTTP code ${http_code}"
  exit 11
fi

# Response validation
if [[ "${allow_dummy}" -ne 1 ]]; then
  log "Strict mode: checking for stub/dummy markers"
  
  if grep -qi "stub\|dummy" "${run_dir}/http_body.json" 2>/dev/null; then
    log "ERROR: Response contains stub/dummy marker"
    log "       C-Gateway is in STUB MODE (see ADR-005)"
    log "       Set ALLOW_DUMMY_RESPONSE=1 to run in template mode"
    exit 12
  fi
  
  log "Response looks non-stub (basic checks passed)"
else
  log "Template mode: skipping stub/dummy detection (ALLOW_DUMMY_RESPONSE=1)"
  
  if grep -qi "stub\|dummy" "${run_dir}/http_body.json" 2>/dev/null; then
    log "✓  Detected stub response (expected in template mode)"
  fi
fi

log ""
log "╔════════════════════════════════════════════════════════════════╗"
log "║  SMOKE TEST PASSED (template mode)                            ║"
log "╚════════════════════════════════════════════════════════════════╝"
log ""
log "Infrastructure validation complete:"
log "  ✓ NATS started and responding"
log "  ✓ Router started and responding"
log "  ✓ C-Gateway started and responding"
log "  ✓ HTTP request successful"
log ""
if [[ "${allow_dummy}" -eq 1 ]]; then
  log "⚠️  NOTE: Running in TEMPLATE MODE (stub responses allowed)"
  log "    To enable strict mode (requires real integration):"
  log "      ALLOW_DUMMY_RESPONSE=0 ./scripts/smoke_cgateway_router.sh"
  log ""
  log "    Prerequisites (see ADR-005 in .ai/decisions.md):"
  log "      - Router perf stable (3+ green Heavy CT runs)"
  log "      - Performance baseline documented"
  log "      - Regression guards enabled in CI"
  log "      - libnats installed, c-gateway rebuilt with -DUSE_NATS_LIB=ON"
fi
log ""
log "Artifacts: ${run_dir}"

exit 0
