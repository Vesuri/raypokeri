#!/usr/bin/env bash
# Launch FS-UAE gdb-stub, connect gdb, let Pokeri run, SIGINT gdb after a delay so it
# breaks in and runs the read-only print commands in $GDBSCRIPT (default diag.gdb).
set -uo pipefail
cd "$(dirname "$0")"
RUN="${POKERI_RUN_DIR:-.run}"
FSUAE_RUN="${FSUAE_RUN:-$RUN}"
. "${FSUAE_COMMON:-$HOME/.local/share/amiga/fsuae_common.sh}"
FSUAE="${FSUAE:-fs-uae}"
GDB="${GDB:-m68k-amiga-elf-gdb}"
ROM="${KICKSTART:-$HOME/Documents/RetroPie/BIOS/kick31.rom}"
DELAY="${1:-14}"
# A1200 bring-up until gameplay works; use AMIGA_MODEL=A500+ for later optimization.
MODEL="${AMIGA_MODEL:-A1200}"
# Optional extra fs-uae args, e.g. EXTRA_ARGS="--cpu=68040 --jit_compiler=1".
EXTRA_ARGS="${EXTRA_ARGS:-}"

DH0="$RUN/dh0"; DH1="$RUN/dh1"; GDBHOME="$RUN/gdbhome"
mkdir -p "$DH0/s" "$DH1" "$RUN/state" "$GDBHOME"
# ⚠ Do NOT "simplify" this to a single `dh1:Pokeri` line.  Measured in Rescue on Fractalus 2026-08-14: with
# the path form, gdb resolves this file's symbols against base $7500 instead of the usual
# ~$21f8e0 and then NO breakpoint is ever hit, so every run looks like a hang.  ($7500 is
# also what you get when the program never loads at all, so the likeliest reading is that
# --remote_debugger_trigger=Pokeri stops matching and no segment base is ever reported — but
# only the symptom was confirmed, not the mechanism.)  The `cd` is load-bearing; leave it.
# (Consequence: this harness needs KS 2.0+, since `cd` is only a ROM-resident Shell builtin
# from 2.0 on — a KS 1.3 boot dies here with "Unknown command cd".  Pokeri is
# not yet established to be 1.3-clean.)
# Replay is explicit; normal launches do not read or allocate replay.bin.
if [ "${POKERI_REPLAY:-1}" = 1 ]; then
  touch "$DH1/native-replay"
else
  rm -f "$DH1/native-replay"
fi
# Explicit development seed only; never overwrite a live save or seed replay.
if [ -n "${POKERI_ACCOUNTING_SEED:-}" ] && [ "${POKERI_REPLAY:-1}" != 1 ]; then
  python3 ../host/native_warm_fixture.py --seed "$POKERI_ACCOUNTING_SEED" --drive "$DH1" || exit 1
fi
printf 'cd dh1:\nPokeri\n' > "$DH0/s/startup-sequence"
cp -f "${POKERI_EXE:-out/Pokeri}" "$DH1/Pokeri"
cp -f "${POKERI_ELF:-out/Pokeri.elf}" "$RUN/Pokeri.elf"
cp -f "${GDBSCRIPT:-diag.gdb}" "$RUN/diagnostic.gdb"
python3 ../host/release_probe.py --elf "$RUN/Pokeri.elf" \
  --template "$RUN/diagnostic.gdb" --out "$RUN/diagnostic.gdb" || exit 1

fsuae_claim_port
# Discard host audio during debugging; keep emulated Paula running.
SDL_AUDIODRIVER=dummy "$FSUAE" \
  --amiga_model="$MODEL" --chip_memory=1024 --fast_memory=8192 \
  --kickstart_file="$ROM" \
  --hard_drive_0="$DH0" --hard_drive_1="$DH1" \
  --automatic_input_grab=0 --fullscreen=0 --window_width=720 --window_height=568 \
  $EXTRA_ARGS \
  --remote_debugger=20 --remote_debugger_port="$DEBUG_PORT" --remote_debugger_trigger=Pokeri \
  --ntsc_mode=0 --state_dir="$RUN/state" > "$RUN/fsuae-dbg.log" 2>&1 &
FSUAE_PID=$!
fsuae_track "$FSUAE_PID"
echo "FS-UAE pid=$FSUAE_PID; waiting for stub..."
for i in $(seq 1 60); do
  kill -0 "$FSUAE_PID" 2>/dev/null || { echo "FS-UAE exited early; see $RUN/fsuae-dbg.log"; exit 1; }
  lsof -nP -iTCP:"$DEBUG_PORT" -sTCP:LISTEN >/dev/null 2>&1 && break
  sleep 1
done

cat > "$RUN/connect.gdb" <<EOF
set pagination off
set confirm off
set remotetimeout 90
target remote 127.0.0.1:$DEBUG_PORT
EOF

env HOME="$GDBHOME" XDG_CACHE_HOME="$GDBHOME" \
  "$GDB" -q -l 10 -x "$RUN/connect.gdb" -x "$RUN/diagnostic.gdb" "$RUN/Pokeri.elf" \
  > "$RUN/gdb-out.log" 2>&1 &
GDB_PID=$!
echo "gdb pid=$GDB_PID; running for ${DELAY}s..."
# Completion breakpoints may finish well before the maximum diagnostic budget.
for i in $(seq 1 "$DELAY"); do
  kill -0 "$GDB_PID" 2>/dev/null || break
  sleep 1
  if [ "${PROGRESS_INTERVAL:-0}" -gt 0 ] && [ $((i % PROGRESS_INTERVAL)) -eq 0 ]; then
    kill -INT "$GDB_PID" 2>/dev/null || true
  fi
done
kill -INT "$GDB_PID" 2>/dev/null || true
# give gdb time to print + detach
for i in $(seq 1 20); do kill -0 "$GDB_PID" 2>/dev/null || break; sleep 1; done
kill -INT "$GDB_PID" 2>/dev/null || true
sleep 2
kill -9 "$GDB_PID" 2>/dev/null || true
fsuae_stop
echo "=== gdb output (filtered) ==="
grep -v "Internal error: pc" "$RUN/gdb-out.log" | grep -vE "^warning:" | tail -40
