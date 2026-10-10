#!/usr/bin/env bash
set -euo pipefail

repo_name="Xiaomi-Kernel-OpenSource-1.1"

printf '%s\n' "Repository: ${repo_name}"
printf '%s\n' "Current branch: $(git branch --show-current 2>/dev/null || echo unknown)"
printf '\nAvailable branches:\n'

git branch -a | sed 's/^..//g' | head -n 50

printf '\nExample device branches:\n'
for b in alioth-r-oss begonia-r-oss jasmine-q-oss taoyao-s-oss violet-q-oss; do
  if git show-ref --verify --quiet "refs/heads/$b" || git show-ref --verify --quiet "refs/remotes/origin/$b"; then
    printf '%s\n' "$b"
  fi
done
