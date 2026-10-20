#!/usr/bin/env bash
# Exercise the collision-search functions and the failure paths of
# scripts/cpc.sh and scripts/generic_ipc.sh; never run a full CPC attack.
set -u
repo=$(cd "$(dirname "$0")/.." && pwd)
scratch=$(mktemp -d "${TMPDIR:-/tmp}/hashclash-cpc.XXXXXX") || exit 1
trap 'rm -rf "$scratch"' EXIT
sed -n '/^function testcoll {/,/^function auto_kill/{ /^function auto_kill/d; p; }' \
    "$repo/scripts/cpc.sh" > "$scratch/functions.sh"
source "$scratch/functions.sh"
trap ':' TERM
original_term=$(trap -p TERM)
cat > "$scratch/helper" <<'STUB'
#!/usr/bin/env bash
printf '%s\n' "$@" > "$ARGS"
echo $$ > "$HELPER_PID"
case "$MODE" in
    fail) echo helper-failed; exit 23 ;;
    empty) echo no-collision; exit 0 ;;
    success) touch "$WORK/coll1_test" "$WORK/coll2_test"; exec sleep 30 ;;
    finished) touch "$WORK/coll1_test" "$WORK/coll2_test"; exit 0 ;;
    interrupt) exec sleep 30 ;;
esac
STUB
chmod +x "$scratch/helper"
export HELPER="$scratch/helper" CPUS=1 ARGS="$scratch/args" HELPER_PID="$scratch/helper.pid"
fail() { echo "FAIL: $*" >&2; exit 1; }
for MODE in fail empty success finished interrupt; do
    export MODE WORK="$scratch/$MODE"
    mkdir -p "$WORK/connect"
    if [ "$MODE" = interrupt ]; then
        docollfind "$WORK" "$WORK/connect" &
        searchpid=$!
        for ((i=0; i<50; ++i)); do
            [ -f "$HELPER_PID" ] && break
            sleep 0.1
        done
        [ -f "$HELPER_PID" ] || fail "helper did not start"
        kill -TERM "$searchpid"
        wait "$searchpid"
        status=$?
    else
        docollfind "$WORK" "$WORK/connect"
        status=$?
    fi
    case "$MODE:$status" in
        fail:23|empty:1|success:0|finished:0|interrupt:143) ;;
        *) fail "$MODE returned $status" ;;
    esac
    [ "$(trap -p TERM)" = "$original_term" ] || fail "caller TERM trap changed"
    grep -qx -- --findcollision "$ARGS" || fail "missing full option name"
    if kill -0 "$(cat "$HELPER_PID")" 2>/dev/null; then
        fail "$MODE left the helper running"
    fi
    [ -z "$(find "$WORK" -name '.collfind.*' -print)" ] || fail "FIFO directory leaked"
    case "$MODE" in
        fail) grep -q helper-failed "$WORK/connect/collfind.log" || fail "missing failure log" ;;
        empty) grep -q no-collision "$WORK/connect/collfind.log" || fail "missing empty log" ;;
    esac
    rm -f "$HELPER_PID"
    echo "PASS: $MODE"
done
# killworkers() must terminate the md5_* workers on every platform and leave
# unrelated processes alone ('killall -r' is a GNU extension that the BSD
# killall of macOS rejects, which used to leave every worker running).
BINDIR="$scratch/bin"
mkdir -p "$BINDIR"
for tool in md5_diffpathhelper md5_diffpathconnect; do
    ln -s "$(command -v sleep)" "$BINDIR/$tool"
done
"$BINDIR/md5_diffpathhelper" 60 & worker1=$!
"$BINDIR/md5_diffpathconnect" 60 & worker2=$!
sleep 60 & control=$!
sleep 1
killworkers
wait "$worker1" "$worker2" 2>/dev/null
sleep 1
kill -0 "$worker1" 2>/dev/null && fail "killworkers left $worker1 running"
kill -0 "$worker2" 2>/dev/null && fail "killworkers left $worker2 running"
kill -0 "$control" 2>/dev/null || fail "killworkers killed an unrelated process"
kill -9 "$control" 2>/dev/null
wait "$control" 2>/dev/null
echo 'PASS: killworkers'
# The caller must mark the step as failed instead of assembling nonexistent blocks.
WORK="$scratch/step"
mkdir "$WORK"
(
    cd "$WORK" || exit 1
    file1=prefix1
    file2=prefix2
    echo config > stub.cfg
    HELPER=true
    doforward() { :; }
    dobackward() { :; }
    doconnect() { :; }
    docollfind() { return 23; }
    dostepk 0
)
status=$?
[ "$status" -ne 0 ] || fail "dostepk swallowed the search failure"
[ -f "$WORK/workdir0/killed" ] || fail "dostepk did not mark the failed step"
[ ! -e "$WORK/file1_1.bin" ] || fail "dostepk assembled an invalid collision"
echo 'PASS: dostepk failure propagation'

# generic_ipc.sh must not report success when the collision finder fails: the
# helper returns 0 after swallowing a worker exception, and an unmatched glob
# used to be handed to cat as a literal filename, so a failed run concatenated
# truncated data and exited 0.
cat > "$scratch/tool" <<'STUB'
#!/usr/bin/env bash
case "$(basename "$0")" in
    md5_diffpathhelper)
        for arg in "$@"; do
            if [ "$arg" = --findcollision ]; then
                echo 'Worker thread: caught exception:'
                echo 'stub: collision finding failed'
                exit 23
            fi
        done
        touch data/dummy.prefix.bin.base data/path_prefix.bin data/path_2nc.bin
        ;;
    md5_diffpathconnect)
        mkdir -p data
        echo stub-bestpath > data/bestpath.bin.gz
        ;;
esac
echo 'Q32: 0 0 0 0'
exit 0
STUB
chmod +x "$scratch/tool"
mkdir -p "$scratch/stubbin"
for tool in md5_diffpathhelper md5_diffpathconnect md5_diffpathforward md5_diffpathbackward; do
    ln -s "$scratch/tool" "$scratch/stubbin/$tool"
done
sed -n -e '/^function findcollision$/,/^}/p' -e '/^function checkcollisionblocks$/,/^}/p' \
    "$repo/scripts/generic_ipc.sh" > "$scratch/ipc_functions.sh"
# shellcheck disable=SC1090
source "$scratch/ipc_functions.sh"
IPC="$scratch/ipc"
mkdir -p "$IPC/data"
diff='--diffm2 9'
HELPER="$scratch/stubbin/md5_diffpathhelper"
( cd "$IPC" && findcollision collfind.log bestpath.bin.gz ) \
    > /dev/null 2> "$scratch/ipc.err"
status=$?
[ "$status" -eq 23 ] || fail "findcollision returned $status instead of the helper status"
grep -q 'Worker thread' "$IPC/collfind.log" || fail "helper output was not logged"
grep -q 'collision finding failed with exit status 23' "$scratch/ipc.err" ||
    fail "findcollision reported no failure"
( cd "$IPC" && checkcollisionblocks ) 2> "$scratch/blocks.err"
status=$?
[ "$status" -ne 0 ] || fail "checkcollisionblocks accepted missing collision blocks"
grep -q 'coll1_' "$scratch/blocks.err" || fail "no diagnostic for the missing block"
touch "$IPC/data/coll1_stub" "$IPC/data/coll2_stub"
( cd "$IPC" && checkcollisionblocks ) || fail "checkcollisionblocks rejected existing blocks"
WORK="$scratch/ipcrun"
mkdir -p "$WORK"
( cd "$WORK" && HASHCLASH_BIN="$scratch/stubbin" bash "$repo/scripts/generic_ipc.sh" ) \
    > "$scratch/ipcrun.out" 2> "$scratch/ipcrun.err"
status=$?
[ "$status" -ne 0 ] || fail "generic_ipc.sh reported success after a failed collision search"
[ ! -e "$WORK/collision1.bin" ] || fail "generic_ipc.sh concatenated a bogus collision"
[ ! -e "$WORK/data/file2.bin" ] || fail "generic_ipc.sh concatenated missing collision blocks"
grep -q 'collision finding failed with exit status 23' "$scratch/ipcrun.err" ||
    fail "generic_ipc.sh reported no failure"
echo 'PASS: generic_ipc.sh failure propagation'

# Exercise the release, development, legacy and fallback lookup order.
pathsroot="$scratch/paths"
mkdir -p "$pathsroot/scripts"
pathsroot=$(cd "$pathsroot" && pwd)
cp "$repo/scripts/hashclash_paths.sh" "$pathsroot/scripts/"
for preset in dev dev-tsan dev-asan release; do
    mkdir -p "$pathsroot/build/$preset/bin"
    touch "$pathsroot/build/$preset/bin/md5_diffpathhelper"
    chmod +x "$pathsroot/build/$preset/bin/md5_diffpathhelper"
done
check_bindir() {
    local actual
    actual=$(unset HASHCLASH_BIN; source "$pathsroot/scripts/hashclash_paths.sh"; printf '%s' "$HASHCLASH_BIN")
    [ "$actual" = "$1" ] || fail "binary lookup selected '$actual' instead of '$1'"
}
check_bindir "$pathsroot/build/release/bin"
rm -rf "$pathsroot/build/release"
check_bindir "$pathsroot/build/dev/bin"
mkdir -p "$pathsroot/bin"
touch "$pathsroot/bin/md5_diffpathhelper"
chmod +x "$pathsroot/bin/md5_diffpathhelper"
check_bindir "$pathsroot/build/dev/bin"
mkdir -p "$pathsroot/build/cmake-ninja/bin"
touch "$pathsroot/build/cmake-ninja/bin/md5_diffpathhelper"
chmod +x "$pathsroot/build/cmake-ninja/bin/md5_diffpathhelper"
rm -rf "$pathsroot/build/dev"
check_bindir "$pathsroot/build/cmake-ninja/bin"
rm -rf "$pathsroot/build/cmake-ninja"
check_bindir "$pathsroot/bin"
rm -rf "$pathsroot/bin"
check_bindir "$pathsroot/build/dev-asan/bin"
(
    HASHCLASH_BIN="$pathsroot/custom"
    source "$pathsroot/scripts/hashclash_paths.sh"
    [ "$HASHCLASH_BIN" = "$pathsroot/custom" ]
) || fail 'explicit HASHCLASH_BIN was overridden'
echo 'PASS: binary directory priority and override'
