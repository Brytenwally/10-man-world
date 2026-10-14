#!/usr/bin/env bash
set -euo pipefail

patch_file="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/mod-10-man-world-core.patch"
core_dir="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)}"

cd "$core_dir"

if git apply --reverse --check "$patch_file" 2>/dev/null; then
  echo "mod-10-man-world: core patch already applied in $core_dir"
  exit 0
fi

git apply --check "$patch_file"
git apply "$patch_file"
echo "mod-10-man-world: core patch applied in $core_dir"
