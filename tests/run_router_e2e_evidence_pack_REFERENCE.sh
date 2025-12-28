#!/usr/bin/env bash
# run_router_e2e_evidence_pack.sh
# User's complete implementation - saved as reference

set -euo pipefail

ts="$(date +%Y%m%d_%H%M%S)"
root="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "${root}" ]]; then
  echo "ERROR: must run inside a git repo" >&2
  exit 2
fi

# Config (override via ENV)
ipc_socket_path="${IPC_SOCKET_PATH:-/tmp/beamline-gateway.sock}"
nats_url="${NATS_URL:-nats://127.0.0.1:4222}"
router_subject="${ROUTER_SUBJECT:-beamline.router.v1.decide}"

artifacts_dir="${ARTIFACTS_DIR:-${root}/artifacts/router-e2e/${ts}}"
mkdir -p "${artifacts_dir}"

checks_tsv="${artifacts_dir}/checks.tsv"
router_log="${artifacts_dir}/router.log"
gateway_log="${artifacts_dir}/gateway.log"
client_log="${artifacts_dir}/ipc_client.log"

echo -e "check_id\tstatus\tdetails\tevidence_path" > "${checks_tsv}"

write_check() {
  local check_id="$1" status="$2" details="$3" evidence_path="$4"
  echo -e "${check_id}\t${status}\t${details}\t${evidence_path}" >> "${checks_tsv}"
}

# === Meta capture ===
{
  echo "timestamp=${ts}"
  echo "root=${root}"
  echo "ipc_socket_path=${ipc_socket_path}"
  echo "nats_url=${nats_url}"
  echo "router_subject=${router_subject}"
} > "${artifacts_dir}/meta.env"

{
  echo "uname -a: $(uname -a)"
  command -v gcc >/dev/null 2>&1 && echo "gcc: $(gcc --version | head -n1)" || echo "gcc: not found"
} > "${artifacts_dir}/meta.versions"

(
  cd "${root}"
  echo "commit=$(git rev-parse HEAD)"
  echo "branch=$(git rev-parse --abbrev-ref HEAD)"
) > "${artifacts_dir}/meta.git"

# === FACTS NEEDED FROM USER ===
# 1. Gateway start command
# 2. Router start command  
# 3. Router subject

# PLACEHOLDER - will update with real commands
echo "NOTE: This is User's reference implementation" > "${artifacts_dir}/README.txt"
echo "Needs 3 facts from user:" >> "${artifacts_dir}/README.txt"
echo "1. Gateway start command" >> "${artifacts_dir}/README.txt"
echo "2. Router start command" >> "${artifacts_dir}/README.txt"
echo "3. Router subject" >> "${artifacts_dir}/README.txt"

# See User's message for complete implementation
