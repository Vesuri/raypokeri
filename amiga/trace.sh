#!/usr/bin/env bash
# Record FS-UAE (Barto gdb stub) cycle-exact instruction traces of the native
# game, then attribute them with host/native_trace.py. Diagnostic only: the
# traced executable has no timers or counters; TRACE_CODE only moves the Board
# storage into the first code hunk so original instructions keep their PCs.
#
#   ./trace.sh prepare NAME             preparation before original execution
#   ./trace.sh startup NAME [WARM_DH1]   cold (or warm: copy accounting.bin/nvram.bin
#                                        from WARM_DH1) first instruction -> Ready
#   ./trace.sh play NAME                 deal, draw and first accepted Double
#   TRACE_FLAGS="NAME=VALUE ..."         extra build flags for a comparison build
#
# Each 100-field capture is about 1 GB under .run/NAME. Builds are cleaned and
# the normal executable is rebuilt afterwards. Debug audio stays muted.
set -euo pipefail
cd "$(dirname "$0")"
mode="${1:?prepare|startup|play}" name="${2:?run name}" warm="${3:-}"
case "$mode" in
  prepare) flags=(TRACE_CODE=1) script=trace-prepare.gdb prefix=prepare ;;
  startup) flags=(TRACE_CODE=1) script=trace-startup.gdb prefix=startup ;;
  play) flags=(TRACE_CODE=1 DOUBLE_SCENARIO=1) script=trace-play.gdb prefix= ;;
  *) echo "mode must be prepare, startup or play" >&2; exit 2 ;;
esac
# Optional comparison build flags, e.g. TRACE_FLAGS=HANDLER_JOINED=1.
flags+=(${TRACE_FLAGS:-})
. ./env.sh >/dev/null
run="$(pwd)/.run/$name" bin="$(pwd)/.run/$name-bin"
rm -rf "$run" "$bin"; mkdir -p "$run/dh1" "$bin"
cp -R ../rom "$run/dh1/rom"
[ "$mode" = play ] && touch "$run/dh1/native-test-inputs"
if [ -n "$warm" ]; then cp "$warm/accounting.bin" "$warm/nvram.bin" "$run/dh1/"; fi
sed "s|@OUT@|$run|g" "$script" > "$run/run.gdb"
make clean >/dev/null; make -j8 "${flags[@]}" > "$bin/build.log" 2>&1
cp out/Pokeri out/Pokeri.elf "$bin/"
make clean >/dev/null; make -j8 > "$bin/build-normal.log" 2>&1 || true
DEBUG_PORT="${DEBUG_PORT:-3221}" POKERI_REPLAY=0 POKERI_RUN_DIR="$run" AMIGA_MODEL="${AMIGA_MODEL:-A1200}" \
  GDBSCRIPT="$run/run.gdb" EXTRA_ARGS=--warp_mode=1 POKERI_EXE="$bin/Pokeri" POKERI_ELF="$bin/Pokeri.elf" \
  ./diag_run.sh "${TRACE_TIMEOUT:-4000}"
grep -a "^TRACE" "$run/gdb-out.log" || true
python3 ../host/native_trace.py --run "$run" ${prefix:+--prefix "$prefix"} --tree 1.0
