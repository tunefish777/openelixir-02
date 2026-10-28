# OpenElixir-02

[![Build](https://github.com/tunefish777/openelixir-02/actions/workflows/build.yml/badge.svg)](https://github.com/tunefish777/openelixir-02/actions/workflows/build.yml)

Custom firmware for the Erica Synths LXR-02 drum machine. It is based on the open-source
[LXR V2 firmware](https://github.com/erica-synths/lxr-desktop-app) by Erica Synths.

> OpenElixir-02 is not official Erica Synths firmware and is not affiliated with or approved by
> Erica Synths. You install it at your own risk.

[![Watch the video](https://img.youtube.com/vi/mmoUcfnjkb8/hqdefault.jpg)](https://www.youtube.com/embed/<VIDEO_ID>)

[<img src="https://img.youtube.com/vi/mmoUcfnjkb8/hqdefault.jpg" width="600" height="300"/>](https://www.youtube.com/embed/mmoUcfnjkb8)


## Goals

- **More functionality, fully backwards compatible.** OpenElixir-02 extends what is already there.
  Kits, patterns and projects from the stock firmware load and play as before, the MIDI
  implementation is unchanged, and the update works with the stock bootloader.
- **The stock sound stays available.** Every change to the core sound of the unit comes with a
  switch in the config menu, so the stock sound is always one setting away.

## Features

**Kit management.** Load full kits and single voices from any project:
- Hold LOAD and press step button 1-7 (Drum 1 ... OP HH) to load just that voice from any kit of
  any project into the current kit.
- "Change Project" below kit 0 in the load and save kit menus loads kits from other projects and
  saves kits into them.

**Freely assignable voice engines.** Each of the 6 voice slots can run any engine: drum, snare,
cymbal or hi-hat. Select a voice and press SHIFT + KIT (or send NRPN 98). The engines are saved in
the kit; kits from the stock firmware load with the stock engines.

Details for both: [docs/ENGINES.md](docs/ENGINES.md).

**Limiter.** Voices and FX are summed in 32 bit, followed by a limiter on each output, so loud kits
no longer hard-clip. Levels that didn't clip before are unchanged. Config menu "lim" (2nd page)
switches it off for exactly the stock mix.

**Under the hood.** About 10 % less CPU load, many stock firmware bugs fixed (MIDI, sequencer, SD
card, memory corruption; most found in the firmware review by jhw in
[erica-synths/lxr-desktop-app#3](https://github.com/erica-synths/lxr-desktop-app/issues/3)), and a
project save that can't lose the saved project. Stock issues that are kept on purpose:
[docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md). The changes of each version are in the
[release notes](https://github.com/tunefish777/openelixir-02/releases).

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
