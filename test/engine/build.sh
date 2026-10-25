#!/bin/bash
# Usage: build.sh <repo root> <output dir>
# Compiles the audio engine sources of <repo root> for the host (strict IEEE
# float, no fast-math, so refactorings that keep the arithmetic give identical
# bits) and links them with the harness into <output dir>/render.
set -e
ROOT=$(cd "$1" && pwd); OUT=$2; HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT/obj"
CFLAGS="-O1 -g -fno-fast-math -ffp-contract=off -fno-strict-aliasing -fcommon -w -DSTM32F765xx -DUSE_HAL_DRIVER -DFIRMWARE_NAME=\"OE\" -D__fp16=_Float16 -include $HERE/shim/host_cmsis.h"
INC="-I$HERE/shim -I$ROOT/Core/Inc -I$ROOT/Drivers/CMSIS/Include -I$ROOT/Drivers/CMSIS/Device/ST/STM32F7xx/Include -I$ROOT/Drivers/STM32F7xx_HAL_Driver/Inc -I$ROOT/Middlewares/JS/STM32_USB_Device_Library/Core/Inc -I$ROOT/Middlewares/JS/STM32_USB_Device_Library/Class/CDC/Inc -I$ROOT/USB_DEVICE_MIDI/Target -I$ROOT/USB_DEVICE_MIDI/App -I$ROOT/FATFS/App -I$ROOT/FATFS/Target -I$ROOT/Middlewares/Third_Party/FatFs/src"
SRCS=$(ls "$ROOT"/Core/Src/Soundengine/DSPAudio/*.c "$ROOT"/Core/Src/Soundengine/DSPAudio/FX/*.c "$ROOT"/Core/Src/Soundengine/DSPAudio/FX/VerbTools/*.c)
SRCS="$SRCS $ROOT/Core/Src/Soundengine/FxBus.c $ROOT/Core/Src/Soundengine/MIDI/MidiParser.c $ROOT/Core/Src/Soundengine/MIDI/MidiVoiceControl.c $ROOT/Core/Src/Soundengine/MIDI/ParameterArray.c $ROOT/Core/Src/Soundengine/MIDI/MidiMessages.c $ROOT/Core/Src/Preset/initKit.c $ROOT/Core/Src/Soundengine/MIDI/CortexParser.c $ROOT/Core/Src/Soundengine/Soundengine.c"
# UI side of engine switching (when the tree has it)
if [ -f "$ROOT/Core/Src/Preset/KitEngines.c" ]; then
	SRCS="$SRCS $ROOT/Core/Src/Preset/KitEngines.c $ROOT/Core/Src/menu/Cc2Text.c"
fi
fail=0
for f in $SRCS; do
	o="$OUT/obj/$(basename "$f" .c).o"
	gcc -c $CFLAGS $INC "$f" -o "$o" 2>"$o.err" || { echo "FAIL $f"; head -8 "$o.err"; fail=1; }
done
[ $fail = 0 ] || exit 1
python3 "$HERE/gen_ranges.py" "$ROOT" "$OUT/ranges_gen.c"
for f in "$HERE"/*.c "$OUT/ranges_gen.c"; do
	gcc -c $CFLAGS $INC "$f" -o "$OUT/obj/harness_$(basename "$f" .c).o" 2>"$OUT/obj/harness.err" || { cat "$OUT/obj/harness.err"; exit 1; }
done
gcc -o "$OUT/render" "$OUT"/obj/*.o -lm
