#!/bin/bash
# scripts/check_percent_readiness.sh
#
# D15: Forbid "percent readiness" in docs unless computed from artifacts
# Scans docs/ for percentage claims not backed by artifacts

set -e

echo "========================================"
echo "Checking for Unsupported Readiness Claims"
echo "========================================"
echo ""

DOCS_DIR="docs"
VIOLATIONS=0

# Keywords that indicate subjective readiness claims
FORBIDDEN_PATTERNS=(
    "[0-9]+%.*ready"
    "[0-9]+%.*complete"
    "[0-9]+%.*done"
    "percent.*ready"
    "percentage.*complete"
)

# Allowed patterns (computed from artifacts)
ALLOWED_MARKERS=(
    "computed from artifacts"
    "calculated from"
    "based on checks.tsv"
    "from summary.json"
)

echo "Scanning $DOCS_DIR for subjective readiness claims..."
echo ""

for pattern in "${FORBIDDEN_PATTERNS[@]}"; do
    while IFS= read -r match; do
        file=$(echo "$match" | cut -d: -f1)
        line=$(echo "$match" | cut -d: -f2)
        content=$(echo "$match" | cut -d: -f3-)
        
        # Check if this line has an allowed marker
        has_marker=false
        for marker in "${ALLOWED_MARKERS[@]}"; do
            if echo "$content" | grep -qi "$marker"; then
                has_marker=true
                break
            fi
        done
        
        if [ "$has_marker" = false ]; then
            echo "❌ VIOLATION: $file:$line"
            echo "   Content: $content"
            echo "   Missing: 'computed from artifacts' or similar marker"
            echo ""
            ((VIOLATIONS++))
        fi
    done < <(grep -rniE "$pattern" "$DOCS_DIR" 2>/dev/null || true)
done

echo ""
echo "=== Results ==="
echo "Violations found: $VIOLATIONS"
echo ""

if [ $VIOLATIONS -eq 0 ]; then
    echo "✅ PASS: All readiness claims are artifact-based or properly marked"
    exit 0
else
    echo "❌ FAIL: Found $VIOLATIONS subjective readiness claims"
    echo ""
    echo "Fix: Either:"
    echo "  1. Remove the percentage claim"
    echo "  2. Add 'computed from artifacts' marker"
    echo "  3. Actually compute from artifacts (checks.tsv, summary.json)"
    exit 1
fi
