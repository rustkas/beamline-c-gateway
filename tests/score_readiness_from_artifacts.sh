#!/usr/bin/env bash
# score_readiness_from_artifacts.sh - Auto-verdict from TSV artifacts
# User's FINAL specification

set -euo pipefail

dir="${1:-}"
if [[ -z "${dir}" || ! -f "${dir}/checks.tsv" || ! -f "${dir}/readiness.tsv" ]]; then
  echo "Usage: $0 artifacts/router-e2e/<ts>" >&2
  exit 2
fi

core="$(cut -f1 "${dir}/readiness.tsv")"
system="$(cut -f2 "${dir}/readiness.tsv")"
overall="$(cut -f3 "${dir}/readiness.tsv")"

echo "core_pct=${core}"
echo "system_pct=${system}"
echo "overall_pct=${overall}"

# Hard gate: require all SYS_* checks pass
sys_failed="$(awk -F'\t' '$1 ~ /^SYS_/ && $3 != 1 {c++} END{print c+0}' "${dir}/checks.tsv")"

if [[ "${sys_failed}" -gt 0 ]]; then
  echo "verdict=NOT_PRODUCTION (sys_failed=${sys_failed})"
  exit 1
fi

echo "verdict=PRODUCTION_OK"
exit 0
