# OpenElixir-02

[![Build](https://github.com/tunefish777/openelixir-02/actions/workflows/build.yml/badge.svg)](https://github.com/tunefish777/openelixir-02/actions/workflows/build.yml)

Custom firmware for the Erica Synths LXR-02 drum machine. It is based on the open-source
[LXR V2 firmware](https://github.com/erica-synths/lxr-desktop-app) by Erica Synths.

> OpenElixir-02 is not official Erica Synths firmware and is not affiliated with or approved by
> Erica Synths. You install it at your own risk.

## Goals

- **Better sound.** Fix the audible problems of the stock firmware: digital clipping in the
  mix, wavetable glitches at high notes, aliasing and stepping in the oscillators.
- **Stay compatible.** Same hardware, same bootloader and SD-card update procedure. Keep existing
  kits, patterns and the MIDI implementation working wherever possible.
- **Know the limits.** Every build reports how much memory each region uses (ITCM, DTCM, RAM,
  flash) and how that compares with the stock firmware, so changes stay within the MCU's budget.
- **Room for new features.** Possible future work: more flexible voice engines and sample playback.

## Changes from the stock firmware

| Version | Change |
|---|---|
| 0.1 | Voices and FX are summed in 32 bit, followed by a limiter on each output: loud kits no longer hard-clip. Levels that didn't clip before are unchanged. |
| 0.1 | Memory usage report after every build, compared with the stock firmware. |
| 0.1 | Renamed to OpenElixir-02: shown on the start-up screen and as the USB-MIDI device name. |
| 0.2 | Each of the 6 voice slots can run any engine (drum, snare, cymbal, hi-hat): select a voice, SHIFT + KIT, or NRPN 98. Saved in kits; kits from the stock firmware load with the stock engines and sound as before. See [docs/ENGINES.md](docs/ENGINES.md). |
| 0.2 | About 10 % less CPU load in the worst case and 2.6 KB more free fast RAM (ITCM), with identical sound. |
| 0.2 | Fix: switching the FX delay to ping-pong could write up to 32 KB past the delay buffer and corrupt other sound data (also in the stock firmware). |
| 0.2 | Fix: saving a project into an existing project slot renamed kit 1 to ".SNG" (also in the stock firmware). |
| 0.2 | Development: build option `-DLXR_CPU_METER=ON` (CPU load per voice and FX on the display); host test (`test/engine/`) that proves engine changes render bit-identical. |
| next | Load a single voice from another kit, also from other projects: LOAD + step button 1-7 (Drum 1 ... OP HH). See [docs/ENGINES.md](docs/ENGINES.md#loading-a-voice-from-another-kit). |
| next | Load and save kits from and into other projects: "Change Project" below kit 0 in the load and save kit menus. See [docs/ENGINES.md](docs/ENGINES.md#kits-from-other-projects). |
| next | The 0.1 limiter can be switched off in the config menu ("lim", 2nd page): off gives exactly the stock mix, which clips. |

## Building and installing

```sh
cmake --preset release      # add -DARM_TOOLCHAIN_DIR=<toolchain>/bin if arm-none-eabi-gcc isn't on PATH
cmake --build --preset release
```

This creates `build/release/LXR_OpenElixir-02_V<version>.img`. Copy it to the SD card and install it
with the LXR-02's built-in updater (hold the encoder while switching on). The `LXR` prefix in the file
name is required by the updater.

Every push is also built on GitHub Actions: the firmware files are attached to each run in the
[Actions tab](https://github.com/tunefish777/openelixir-02/actions/workflows/build.yml), and the run
summary shows the memory usage compared with the stock firmware.

Requirements, build options, the memory map, troubleshooting and development details are in the
original firmware's README: **[README_LXR.md](README_LXR.md)**.

## Licence

GPL-3.0-only, with the additional terms in [NOTICE.md](NOTICE.md); see [LICENSE](LICENSE). "Erica
Synths", "LXR" and "LXR-02" are trademarks of Erica Synths, and the LXR hardware is their property.
