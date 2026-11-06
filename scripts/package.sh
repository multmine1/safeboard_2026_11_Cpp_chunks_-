#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
project_name="$(basename "$project_dir")"
parent_dir="$(dirname "$project_dir")"
archive_name="Владислав_Агибалов_С++.zip"

cd "$parent_dir"
zip -r -FS "$archive_name" "$project_name" \
  -x "$project_name/.git/*" \
     "$project_name/build*/*" \
     "$project_name/.cache/*" \
     "$project_name/.DS_Store" \
     "$project_name/*.dSYM/*" \
     "$project_name/*.zip"

printf 'Архив: %s\n' "$parent_dir/$archive_name"

