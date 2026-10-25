# Engine render test (host)

Compiles the audio engine (`Core/Src/Soundengine/...`) for the PC and renders 32 kits
(the init kit and 31 random kits within the UI's value ranges) with dense random triggers
on all 7 tracks and parameter changes while voices sound. Every sample of both DACs is
written to a file, so two source trees can be compared bit for bit. Used to prove that
engine refactorings don't change the sound.

```sh
test/engine/compare.sh            # working tree vs. HEAD
test/engine/compare.sh v0.1       # working tree vs. a tag/commit
```

How it works:
- The engine is driven only through entry points that stay stable across refactorings:
  `soundengine_init()`, `frontParser_parseUartData()` (the UI's byte stream: parameters,
  LFO and velocity targets), `voiceControl_noteOn()` and `mixer_calcNextSampleBlock()`.
- Strict IEEE floating point (no `-ffast-math`), so code that keeps the arithmetic gives
  identical bits. On the target, `-ffast-math` may round differently; listen there.
- `shim/host_cmsis.h` replaces the ARM intrinsics (`__QADD16`, `__SSAT`, ...) with C
  versions; peripheral register ranges are mapped as plain memory (`mmap`).
- `stubs.c`: no-op stand-ins for the sequencer, UI, UART, USB and codec. If the link fails
  with new undefined references outside the engine, add stubs there.
- Value ranges are extracted from `menu.c` at build time (`gen_ranges.py`).
- `mutate.sh`: checks that a tiny change in each voice engine is detected.
- `stats.py`: level statistics per kit of a render file.

Linux only (uses `mmap` with fixed addresses). Needs gcc and Python 3.
