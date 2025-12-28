#!/bin/bash
# create_evidence_pack.sh - Facts-only evidence collection
# Usage: ./scripts/create_evidence_pack.sh <test-type>

set -euo pipefail

TEST_TYPE=${1:-"router-e2e"}
TIMESTAMP=$(date -u +%Y%m%d_%H%M%S)
PACK_DIR="artifacts/${TEST_TYPE}/${TIMESTAMP}"

mkdir -p "$PACK_DIR"/{meta,setup,tests}

# === META === (facts only, no narratives)

echo "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')" > "$PACK_DIR/meta/commit.txt"
git status --porcelain > "$PACK_DIR/meta/git_status.txt" 2>/dev/null || echo "unknown" > "$PACK_DIR/meta/git_status.txt"
uname -a > "$PACK_DIR/meta/uname.txt"

cat > "$PACK_DIR/meta/versions.txt" << EOF
gcc: $(gcc --version | head -1)
make: $(make --version | head -1)
nats-server: $(nats-server --version 2>/dev/null || echo 'not found')
erl: $(erl -eval 'erlang:display(erlang:system_info(otp_release)), halt().' -noshell 2>/dev/null || echo 'not found')
EOF

env | sort > "$PACK_DIR/meta/env.txt"

# === SUMMARY.json template ===

cat > "$PACK_DIR/SUMMARY.json" << 'EOF'
{
  "commit": "unknown",
  "router_commit": "unknown",
  "timestamp": "unknown",
  "scenarios": {}
}
EOF

# Update with real commit
COMMIT=$(cat "$PACK_DIR/meta/commit.txt")
UTC_TIME=$(date -u +%Y-%m-%dT%H:%M:%SZ)

jq --arg commit "$COMMIT" --arg time "$UTC_TIME" \
   '.commit = $commit | .timestamp = $time' \
   "$PACK_DIR/SUMMARY.json" > "$PACK_DIR/SUMMARY.json.tmp" \
   && mv "$PACK_DIR/SUMMARY.json.tmp" "$PACK_DIR/SUMMARY.json"

echo "$PACK_DIR"
