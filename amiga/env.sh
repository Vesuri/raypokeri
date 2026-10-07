#!/usr/bin/env bash
# Source this to put the shared ~/.local Amiga toolchain on PATH (the same install the
# Rescue on Fractalus and Vette repos use).
#   . amiga/env.sh
# or from the amiga/ directory:
#   . env.sh
TC="$HOME/.local"
export PATH="$TC/opt/bin:$TC:$TC/fs-uae:$PATH"
export KICKSTART="${KICKSTART:-$HOME/.local/share/amiga/Kickstarts/kick40063.A600}"
