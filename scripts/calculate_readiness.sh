#!/bin/bash
# calculate_readiness.sh - Automatic readiness from SUMMARY.json
# Usage: ./scripts/calculate_readiness.sh <evidence-pack-dir>

set -euo pipefail

PACK_DIR=${1:?"Usage: $0 <evidence-pack-dir>"}
SUMMARY="$PACK_DIR/SUMMARY.json"

if [[ ! -f "$SUMMARY" ]]; then
  echo "ERROR: No SUMMARY.json found" >&2
  exit 1
fi

# Count scenarios
TOTAL=$(jq '.scenarios | length' "$SUMMARY")
PASSED=$(jq '[.scenarios[] | select(.executed == true and .exit_code == 0)] | length' "$SUMMARY")
FAILED=$((TOTAL - PASSED))

# Calculate
if [[ $TOTAL -eq 0 ]]; then
  SYSTEM_SCORE=0
else
  SYSTEM_SCORE=$(echo "scale=2; $PASSED / $TOTAL" | bc)
fi

# Output facts only
cat << EOF
{
  "total_scenarios": $TOTAL,
  "passed": $PASSED,
  "failed": $FAILED,
  "system_score": $SYSTEM_SCORE,
  "production_approved": $([ "$PASSED" -eq "$TOTAL" ] && [ "$TOTAL" -gt 0 ] && echo "true" || echo "false")
}
EOF
