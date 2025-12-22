#!/usr/bin/env bash
#
# tidy_markdown.sh - Idempotently move non-whitelisted markdown files from root to docs/
#
# Usage: ./scripts/tidy_markdown.sh
#
# This script:
# - Moves specific markdown files from repository root to appropriate docs/ subdirectories
# - Is safe to run multiple times (idempotent)
# - Only moves files if they exist in root and don't already exist in target
# - Creates target directories as needed

set -euo pipefail

# Color output for better readability
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory and repo root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$REPO_ROOT"

echo "🧹 C-Gateway Markdown Tidy Script"
echo "=================================="
echo ""

# Define file mappings: source_file → target_directory
declare -A MOVE_MAP=(
  ["IMPLEMENTATION_PLAN.md"]="docs/plans"
  ["README_RATE_LIMITING_POC.md"]="docs/poc"
  ["TODO.md"]="docs/backlog"
)

moved_count=0

# Process each file
for src_file in "${!MOVE_MAP[@]}"; do
  target_dir="${MOVE_MAP[$src_file]}"
  target_path="$target_dir/$src_file"
  
  # Check if source file exists in root
  if [[ ! -f "$src_file" ]]; then
    echo -e "${GREEN}✓${NC} $src_file - already moved or doesn't exist"
    continue
  fi
  
  # Check if file already exists in target (don't overwrite)
  if [[ -f "$target_path" ]]; then
    echo -e "${YELLOW}⚠${NC} $src_file - target already exists at $target_path (skipping)"
    continue
  fi
  
  # Create target directory if needed
  mkdir -p "$target_dir"
  
  # Move the file
  echo -e "${GREEN}→${NC} Moving $src_file → $target_path"
  mv "$src_file" "$target_path"
  
  ((moved_count++))
done

echo ""
if [[ $moved_count -eq 0 ]]; then
  echo -e "${GREEN}✓${NC} All files already in correct locations. Nothing to do."
else
  echo -e "${GREEN}✓${NC} Moved $moved_count file(s) successfully."
fi

exit 0
