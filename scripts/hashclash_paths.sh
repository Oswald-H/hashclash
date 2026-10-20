#!/usr/bin/env bash

HASHCLASH_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
HASHCLASH_ROOT="$(cd -- "$HASHCLASH_SCRIPT_DIR/.." && pwd)"

# Prefer ordinary release/development builds, the legacy layout and packaged
# binaries. Only fall back to other presets when none of those exist.
if [ -n "${HASHCLASH_BIN:-}" ]; then
	:
else
	for HASHCLASH_BINDIR in \
		"$HASHCLASH_ROOT/build/release/bin" \
		"$HASHCLASH_ROOT/build/dev/bin" \
		"$HASHCLASH_ROOT/build/cmake-ninja/bin" \
		"$HASHCLASH_ROOT/bin" \
		"$HASHCLASH_ROOT"/build/*/bin; do
		if [ -x "$HASHCLASH_BINDIR/md5_diffpathhelper" ]; then
			HASHCLASH_BIN="$HASHCLASH_BINDIR"
			break
		fi
	done
	unset HASHCLASH_BINDIR
	HASHCLASH_BIN="${HASHCLASH_BIN:-$HASHCLASH_ROOT/bin}"
fi

export HASHCLASH_ROOT HASHCLASH_BIN
