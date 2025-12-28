#!/bin/bash
# run_router_e2e.sh - Execute Router E2E with facts-only evidence
# Creates: artifacts/router-e2e/<timestamp>/

set -euo pipefail

TIMESTAMP=$(date -u +%Y%m%d_%H%M%S)
PACK_DIR="artifacts/router-e2e/${TIMESTAMP}"

mkdir -p "$PACK_DIR"

# === META ===

# meta.git
cat > "$PACK_DIR/meta.git" << EOF
gateway_commit=$(git rev-parse HEAD 2>/dev/null || echo 'unknown')
router_commit=unknown
dirty=$(git diff --quiet 2>/dev/null && echo 'false' || echo 'true')
EOF

# meta.env
env | sort > "$PACK_DIR/meta.env"

# meta.versions
cat > "$PACK_DIR/meta.versions" << EOF
gcc: $(gcc --version | head -1)
nats-server: $(nats-server --version 2>/dev/null || echo 'not found')
erl: $(erl -eval 'erlang:display(erlang:system_info(otp_release)), halt().' -noshell 2>/dev/null || echo 'not found')
EOF

# run.command
echo "$0 $@" > "$PACK_DIR/run.command"

# === EXECUTE SCENARIOS ===

# Initialize results.jsonl
touch "$PACK_DIR/results.jsonl"

CHECKS_TOTAL=0
CHECKS_PASSED=0

# Scenario 1: happy_path
echo "{\"ts\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"scenario\":\"happy_path\",\"check\":\"basic_request\",\"ok\":false,\"details\":\"not_implemented\"}" >> "$PACK_DIR/results.jsonl"
CHECKS_TOTAL=$((CHECKS_TOTAL + 1))

# Scenario 2: errors
echo "{\"ts\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"scenario\":\"errors\",\"check\":\"4xx_5xx\",\"ok\":false,\"details\":\"not_implemented\"}" >> "$PACK_DIR/results.jsonl"
CHECKS_TOTAL=$((CHECKS_TOTAL + 1))

# Scenario 3: timeouts
echo "{\"ts\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"scenario\":\"timeouts\",\"check\":\"late_reply\",\"ok\":false,\"details\":\"not_implemented\"}" >> "$PACK_DIR/results.jsonl"
CHECKS_TOTAL=$((CHECKS_TOTAL + 1))

# Scenario 4: backpressure
echo "{\"ts\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"scenario\":\"backpressure\",\"check\":\"throttle\",\"ok\":false,\"details\":\"not_implemented\"}" >> "$PACK_DIR/results.jsonl"
CHECKS_TOTAL=$((CHECKS_TOTAL + 1))

# Scenario 5: reconnect
echo "{\"ts\":\"$(date -u +%Y-%m-%dT%H:%M:%SZ)\",\"scenario\":\"reconnect\",\"check\":\"storm\",\"ok\":false,\"details\":\"not_implemented\"}" >> "$PACK_DIR/results.jsonl"
CHECKS_TOTAL=$((CHECKS_TOTAL + 1))

# === SUMMARY ===

# Count actual checks
CHECKS_FAILED=$(grep -c '"ok":false' "$PACK_DIR/results.jsonl" || echo "$CHECKS_TOTAL")
CHECKS_PASSED=$((CHECKS_TOTAL - CHECKS_FAILED))

SYSTEM_GATE_PASS="false"
if [ "$CHECKS_FAILED" -eq 0 ]; then
    SYSTEM_GATE_PASS="true"
fi

SYSTEM_READINESS_PCT=$(echo "scale=1; $CHECKS_PASSED * 100 / $CHECKS_TOTAL" | bc)

cat > "$PACK_DIR/summary.json" << EOF
{
  "ts_start": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "ts_end": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "exit_code": $CHECKS_FAILED,
  "scenarios_total": 5,
  "checks_total": $CHECKS_TOTAL,
  "checks_passed": $CHECKS_PASSED,
  "checks_failed": $CHECKS_FAILED,
  "system_gate_pass": $SYSTEM_GATE_PASS,
  "system_readiness_pct": $SYSTEM_READINESS_PCT
}
EOF

# run.exit_code
echo "$CHECKS_FAILED" > "$PACK_DIR/run.exit_code"

echo "Evidence pack created: $PACK_DIR"
echo "System gate pass: $SYSTEM_GATE_PASS"
echo "Readiness: $SYSTEM_READINESS_PCT%"

exit "$CHECKS_FAILED"
