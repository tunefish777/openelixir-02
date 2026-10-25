#!/bin/bash
# Mutation check of the harness: for each engine, apply one tiny change to a
# fresh copy of the working tree and verify that the rendered output differs.
# Usage: mutate.sh <repo> <reference render.raw> <work dir (must not exist)>
set -e
REPO=$1; REF=$2; WORK=$3; HERE=$(cd "$(dirname "$0")" && pwd)
mkdir "$WORK"
n=0
while IFS='|' read -r file from to; do
	[ -n "$file" ] || continue
	n=$((n+1)); tree="$WORK/tree$n"; mkdir "$tree"
	(cd "$REPO" && git ls-files -co --exclude-standard -z | xargs -0 tar -c) | tar -x -C "$tree"
	python3 -I -c "
import sys
p, a, b = sys.argv[1:4]
s = open(p, newline='').read()
if a not in s: sys.exit('pattern missing: ' + a)
open(p, 'w', newline='').write(s.replace(a, b, 1))" "$tree/$file" "$from" "$to"
	bash "$HERE/build.sh" "$tree" "$WORK/out$n" >/dev/null
	(cd "$WORK/out$n" && ./render m.raw >/dev/null)
	if cmp -s "$WORK/out$n/m.raw" "$REF"; then echo "NOT DETECTED: $file: $from"; else echo "detected:     $file"; fi
done <<'LIST'
Core/Src/Soundengine/DSPAudio/DrumVoice.c|v->velo = vol/127.f;|v->velo = vol/126.f;
Core/Src/Soundengine/DSPAudio/Snare.c|v->velo = vel/127.f;|v->velo = vel/126.f;
Core/Src/Soundengine/DSPAudio/CymbalVoice.c|v->velo = vel/127.f;|v->velo = vel/126.f;
Core/Src/Soundengine/DSPAudio/HiHat.c|v->velo = vel/127.f;|v->velo = vel/126.f;
LIST
