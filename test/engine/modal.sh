#!/bin/bash
# Renders parameter sweeps of the Modal engine's resonator as WAV files for listening.
#
#   test/engine/modal.sh [output dir]      (default: build/modal-test)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(git -C "$HERE" rev-parse --show-toplevel)
OUT=${1:-$REPO/build/modal-test}
mkdir -p "$OUT"
gcc -O2 -std=c99 -DMODAL_MEASURE -Wall -Wextra -I"$REPO/Core/Inc" \
	"$REPO/Core/Src/Soundengine/DSPAudio/ModalResonator.c" "$HERE/modal/modal_render.c" -lm -o "$OUT/modal_render"
"$OUT/modal_render" "$OUT"
echo "WAV files in $OUT"
