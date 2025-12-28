#!/bin/bash
# check_readiness_consistency.sh - Enforce single source of truth
#
# This script ensures readiness percentages only appear in canonical document

set -e

CANONICAL="docs/readiness/TWO_AXIS_CANONICAL.md"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

echo "=============================================="
echo "READINESS CONSISTENCY CHECK"
echo "=============================================="
echo "Canonical source: $CANONICAL"
echo ""

# Check canonical exists
if [ ! -f "$CANONICAL" ]; then
    echo "❌ ERROR: Canonical document not found: $CANONICAL"
    exit 1
fi

echo "✓ Canonical document exists"
echo ""

# Find files with readiness percentages
echo "Searching for readiness percentages outside canonical..."

# Patterns to search for
PATTERNS=(
    'Overall.*[0-9]{2,3}%'
    'Readiness.*[0-9]{2,3}%'
    'System.*[0-9]{2,3}%'
    'Core.*[0-9]{2,3}%'
)

VIOLATIONS=()

for pattern in "${PATTERNS[@]}"; do
    # Search excluding canonical and known-safe locations
    FILES=$(grep -rl -E "$pattern" \
        --include="*.md" \
        --exclude="$(basename $CANONICAL)" \
        --exclude="DEPRECATED_*.md" \
        --exclude="*_VERIFICATION.md" \
        --exclude-dir=".ai" \
        --exclude-dir=".git" \
        --exclude-dir="artifacts" \
        --exclude-dir="_build" \
        . 2>/dev/null || true)
    
    if [ ! -z "$FILES" ]; then
        for file in $FILES; do
            # Check if file links to canonical
            if ! grep -q "$CANONICAL" "$file" 2>/dev/null; then
                VIOLATIONS+=("$file")
            fi
        done
    fi
done

# Remove duplicates
VIOLATIONS=($(printf '%s\n' "${VIOLATIONS[@]}" | sort -u))

if [ ${#VIOLATIONS[@]} -gt 0 ]; then
    echo "❌ CONSISTENCY VIOLATIONS FOUND!"
    echo ""
    echo "The following files contain readiness percentages"
    echo "but do NOT link to the canonical document:"
    echo ""
    
    for file in "${VIOLATIONS[@]}"; do
        echo "  - $file"
        
        # Show the percentage lines
        grep -n -E '(Overall|Readiness|System|Core).*[0-9]{2,3}%' "$file" 2>/dev/null | head -3 | while read line; do
            echo "      $line"
        done
        echo ""
    done
    
    echo "SOLUTION:"
    echo ""
    echo "Option 1: Remove percentages and link to canonical:"
    echo "  See readiness assessment in: $CANONICAL"
    echo ""
    echo "Option 2: Mark file as deprecated:"
    echo "  Rename to DEPRECATED_<filename>.md"
    echo ""
    echo "Option 3: Move to .ai/ directory (working docs)"
    echo ""
    
    exit 1
fi

echo "✓ No readiness consistency violations found"
echo ""

# Additional checks
echo "Checking for common consistency issues..."

# Check for multiple "FINAL" documents
FINAL_DOCS=$(find . -name "*FINAL*.md" -not -path "./.git/*" -not -path "./.ai/*" -not -path "./artifacts/*" 2>/dev/null || true)

if [ ! -z "$FINAL_DOCS" ]; then
    COUNT=$(echo "$FINAL_DOCS" | wc -l)
    if [ $COUNT -gt 3 ]; then
        echo "⚠ WARNING: Found $COUNT files with 'FINAL' in name"
        echo "  Consider consolidating to avoid confusion"
        echo ""
    fi
fi

# Check for duplicate STATUS documents
STATUS_DOCS=$(find docs/ -name "*STATUS*.md" 2>/dev/null | wc -l || echo 0)
if [ $STATUS_DOCS -gt 2 ]; then
    echo "⚠ WARNING: Found $STATUS_DOCS status documents"
    echo "  Consider using single status source"
    echo ""
fi

echo "=============================================="
echo "✓ Readiness consistency check PASSED"
echo "=============================================="
echo ""
echo "Summary:"
echo "  - Canonical source: $CANONICAL"
echo "  - Violations: 0"
echo "  - Consistency: ENFORCED"
echo ""

exit 0
