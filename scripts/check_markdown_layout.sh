#!/usr/bin/env bash
#
# check_markdown_layout.sh - Validate markdown file layout in repository root
#
# Usage: ./scripts/check_markdown_layout.sh
#
# This script:
# - Checks that only whitelisted markdown files exist in repository root
# - Exits 0 if layout is correct
# - Exits 1 if violations found (suitable for CI)
# - Prints clear error messages for any violations

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory and repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

# Define whitelist: markdown files that ARE allowed in root
WHITELIST=(
  "README.md"
  "LICENSE.md"
  "LICENCE.md"
)

echo "🔍 C-Gateway Markdown Layout Check"
echo "==================================="
echo ""

# Find all markdown files in root (non-recursive)
mapfile -t found_md_files < <(find . -maxdepth 1 -type f -name "*.md" -printf "%f\n" | sort)

if [[ ${#found_md_files[@]} -eq 0 ]]; then
  echo -e "${YELLOW}⚠${NC} No markdown files found in root directory"
  exit 0
fi

violations=()

# Check each found file against whitelist
for file in "${found_md_files[@]}"; do
  is_whitelisted=false
  
  for allowed in "${WHITELIST[@]}"; do
    if [[ "$file" == "$allowed" ]]; then
      is_whitelisted=true
      break
    fi
  done
  
  if [[ "$is_whitelisted" == true ]]; then
    echo -e "${GREEN}✓${NC} $file (whitelisted)"
  else
    echo -e "${RED}✗${NC} $file (NOT ALLOWED in root)"
    violations+=("$file")
  fi
done

echo ""

# Report results
if [[ ${#violations[@]} -eq 0 ]]; then
  echo -e "${GREEN}✓ PASS${NC}: All markdown files in root are whitelisted."
  echo ""
  echo "Allowed files: ${WHITELIST[*]}"
  exit 0
else
  echo -e "${RED}✗ FAIL${NC}: Found ${#violations[@]} non-whitelisted markdown file(s) in root:"
  for violation in "${violations[@]}"; do
    echo "  - $violation"
  done
  echo ""
  echo "These files should be moved to appropriate docs/ subdirectories."
  echo "Run: ./scripts/tidy_markdown.sh"
  echo ""
  echo "Allowed files in root: ${WHITELIST[*]}"
  exit 1
fi
