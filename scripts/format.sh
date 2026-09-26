#!/usr/bin/env bash
# Formats all C++ sources in place. Pass --check to fail instead of rewriting.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode=(-i)
[[ "${1:-}" == "--check" ]] && mode=(--dry-run -Werror)

find "$root/src" "$root/tests" "$root/tools" \( -name '*.h' -o -name '*.cpp' \) -print0 |
    xargs -0 "${CLANG_FORMAT:-clang-format}" "${mode[@]}"
