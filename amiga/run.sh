#!/usr/bin/env bash
# Run the Amiga Pokeri build in FS-UAE as an A1200 during bring-up.
#   ./run.sh [path-to-kickstart-rom]
# Use KS 3.1 (auto-boots directory HDs). Left mouse button quits.
# Override ROM via $1 or $KICKSTART.
#
# Run a DIFFERENT binary than out/Pokeri with $POKERI_EXE — handy for A/B-ing two builds by
# eye or ear without rebuilding between each look, e.g.
#   POKERI_EXE=Pokeri-a ./run.sh          vs      POKERI_EXE=Pokeri-b ./run.sh
#
# Optional extra fs-uae args via $EXTRA_ARGS (the Rescue on Fractalus convention).  Raw WinUAE core
# options take a `uae_` prefix and are passed straight to cfgfile_parse_option, which logs
# `Set option <name> = "<value>"` — grep ~/.local/share/fs-uae/fs-uae.log to prove one took.
# Audio knobs that matter here (fs-uae's A500 defaults hide artefacts):
#   --uae_sound_interpol=none   default `anti`; `none` = the raw non-interpolated path
#   --uae_sound_volcnt=true     default false; emulate Paula's volume-PWM raster
#   --uae_sound_frequency=96000 default 44100
set -euo pipefail
cd "$(dirname "$0")"
. "${FSUAE_COMMON:-$HOME/.local/share/amiga/fsuae_common.sh}"

FSUAE="${FSUAE:-fs-uae}"
ROM="${1:-${KICKSTART:-$HOME/Documents/RetroPie/BIOS/kick31.rom}}"
[ -f "$ROM" ] || { echo "Kickstart ROM not found: $ROM  (pass as \$1 or set \$KICKSTART)"; exit 1; }
EXE="${POKERI_EXE:-out/Pokeri}"
# A1200 bring-up until gameplay works; use AMIGA_MODEL=A500+ for later optimization.
MODEL="${AMIGA_MODEL:-A1200}"
EXTRA_ARGS="${EXTRA_ARGS:-}"
[ -f "$EXE" ] || { echo "not found: $EXE  (build first: make, or set \$POKERI_EXE)"; exit 1; }

RUN=.run; DH0="$RUN/dh0"; DH1="$RUN/dh1"
mkdir -p "$DH0/s" "$DH1" "$RUN/state"
# Kept as two lines, the shape Rescue on Fractalus's diag_run.sh depends on (its `cd` form is
# load-bearing there).  Note it makes the script KS 2.0+: `cd` is a
# ROM-resident Shell builtin only from 2.0 on, so `KICKSTART=.../kick13.rom ./run.sh` dies
# with "Unknown command cd".  Whether Pokeri is 1.3-clean is not yet established.
# Replay is explicit; normal launches do not read or allocate replay.bin.
if [ "${POKERI_REPLAY:-0}" = 1 ]; then
  touch "$DH1/native-replay"
else
  rm -f "$DH1/native-replay"
fi
printf 'cd dh1:\nPokeri\n' > "$DH0/s/startup-sequence"
cp -f "$EXE" "$DH1/Pokeri"
echo "running $EXE"

# ⚠ ALWAYS start from a clean FS-UAE state.  The gdb-stub harnesses (debug.sh) share this
# --state_dir, and they leave a .uss saved while the CPU was halted on the grey first frame —
# resuming that makes ANY build look frozen and grey, which has cost hours of false bisecting.
# There is no reason to resume state here (the game needs none), so just wipe it every run.
rm -f "$RUN"/state/*.uss

# Screenshots: this fsemu-core FS-UAE takes them with HOST-KEY + S = hold F12, press S.
# The screenshot code reads the FSEMU_SCREENSHOTS_DIR env var (the --screenshots_output_dir
# config key is parsed but ignored by the fsemu core), so set it here.  Dir must exist.
SHOTS="${FSEMU_SCREENSHOTS_DIR:-$HOME/Pictures/Screenshots}"
mkdir -p "$SHOTS"
export FSEMU_SCREENSHOTS_DIR="$SHOTS"

fsuae_stop_previous
# After the exec this shell IS fs-uae, so record $$ as the emulator pid.
fsuae_track_self
exec "$FSUAE" \
  --amiga_model="$MODEL" \
  --chip_memory=1024 --fast_memory=8192 \
  --kickstart_file="$ROM" \
  --hard_drive_0="$DH0" --hard_drive_1="$DH1" \
  --joystick_port_0=none --joystick_port_1=none \
  --automatic_input_grab=0 --fullscreen=0 --window_width=720 --window_height=568 \
  --ntsc_mode=0 --state_dir="$RUN/state" \
  --screenshots_output_dir="$SHOTS" \
  $EXTRA_ARGS
