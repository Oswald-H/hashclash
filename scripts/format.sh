#!/usr/bin/env bash
#
# Format or verify the formatting of all non-ignored C/C++ sources.
#
# Usage:
#   scripts/format.sh          # rewrite files in place
#   scripts/format.sh --check  # fail if any file is not formatted
#
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
mode="${1:-format}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
	echo "error: $CLANG_FORMAT not found (set CLANG_FORMAT to override)" >&2
	exit 1
fi

files=()
while IFS= read -r file; do
	if [ -f "$ROOT/$file" ]; then
		files+=("$ROOT/$file")
	fi
done < <(
	git -C "$ROOT" ls-files --cached --others --exclude-standard -- \
		'*.c' '*.cc' '*.cpp' '*.cu' '*.h' '*.hh' '*.hpp'
)

if [ "${#files[@]}" -eq 0 ]; then
	echo "no source files found"
	exit 0
fi

case "$mode" in
format)
	echo "formatting ${#files[@]} files with $("$CLANG_FORMAT" --version)"
	"$CLANG_FORMAT" -i --style=file "${files[@]}"
	;;
--check | check)
	echo "checking ${#files[@]} files with $("$CLANG_FORMAT" --version)"
	"$CLANG_FORMAT" --dry-run --Werror --style=file "${files[@]}"
	;;
*)
	echo "usage: $0 [--check]" >&2
	exit 2
	;;
esac
