#!/bin/bash
# Renders the audio engine of a base commit and of the working tree on the host
# and compares the output sample for sample.
#
#   test/engine/compare.sh [base-ref]      (default: HEAD)
#
# Exit status 0 = bit-identical. Work files go to build/engine-test/.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(git -C "$HERE" rev-parse --show-toplevel)
BASE=${1:-HEAD}
SHA=$(git -C "$REPO" rev-parse --short "$BASE")
WORK="$REPO/build/engine-test"
mkdir -p "$WORK"

if [ ! -d "$WORK/src-$SHA" ]; then
	mkdir "$WORK/src-$SHA"
	git -C "$REPO" archive "$SHA" | tar -x -C "$WORK/src-$SHA"
fi

echo "building base ($BASE = $SHA) ..."
bash "$HERE/build.sh" "$WORK/src-$SHA" "$WORK/out-$SHA"
echo "building working tree ..."
bash "$HERE/build.sh" "$REPO" "$WORK/out-work"

(cd "$WORK/out-$SHA" && ./render render.raw > hashes.txt)
(cd "$WORK/out-work" && ./render render.raw > hashes.txt)

if cmp -s "$WORK/out-$SHA/render.raw" "$WORK/out-work/render.raw"; then
	echo "IDENTICAL: $(wc -l < "$WORK/out-work/hashes.txt") kits, $(stat -c %s "$WORK/out-work/render.raw") bytes"
else
	echo "DIFFERENT. Per kit (base | working tree):"
	paste "$WORK/out-$SHA/hashes.txt" "$WORK/out-work/hashes.txt" | awk '$3 != $6 {print "  " $0}'
	exit 1
fi
