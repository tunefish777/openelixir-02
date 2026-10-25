"""Per-kit level statistics of a render.raw file: stats.py <file> [kits] [blocks]"""
import struct, math, sys
d = open(sys.argv[1], 'rb').read()
kits = int(sys.argv[2]) if len(sys.argv) > 2 else 32
per = (int(sys.argv[3]) if len(sys.argv) > 3 else 400) * 512
for k in range(kits):
    s = struct.unpack('<%dh' % (per // 2), d[k * per:(k + 1) * per])
    nz = sum(1 for x in s if x)
    rms = math.sqrt(sum(x * x for x in s) / len(s))
    last = max([i // 256 for i, x in enumerate(s) if x] or [-1])
    print('kit %2d  nonzero %5.1f%%  rms %7.1f  peak %5d  last sound block %3d'
          % (k, 100 * nz / len(s), rms, max(abs(x) for x in s), last))
