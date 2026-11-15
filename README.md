# OpenElixir-02

[![Build](https://github.com/tunefish777/openelixir-02/actions/workflows/build.yml/badge.svg)](https://github.com/tunefish777/openelixir-02/actions/workflows/build.yml)

Custom firmware for the Erica Synths LXR-02 drum machine. It is based on the open-source
[LXR V2 firmware](https://github.com/erica-synths/lxr-desktop-app) by Erica Synths.

> OpenElixir-02 is not official Erica Synths firmware and is not affiliated with or approved by
> Erica Synths. You install it at your own risk.

## Goals

- **Sound and timing come first.** Nothing has a higher priority than uninterrupted audio and a
  steady sequencer: menus, the display, the SD card and MIDI all wait for them.
- **More functionality, fully backwards compatible.** OpenElixir-02 extends what is already there.
  Kits, patterns and projects from the stock firmware load and play as before, the MIDI
  implementation is unchanged, and the update works with the stock bootloader.
- **The stock sound stays available.** Every change to the core sound of the unit comes with a
  switch in the config menu, so the stock sound is always one setting away.

## Features

### Flexible kit and pattern management
- Load full kits, single voices (hold LOAD + step button 1-7) and patterns from any project, and save
  kits and patterns into any project ("Change Project" at the top of the kit and pattern lists).
- The pattern load and save pages list all 64 slots with their names on the encoder. Lit step buttons
  show the slots with steps; the playing pattern blinks slowly, the next one fast.
- Patterns have names, and saving over a used slot asks for confirmation.

### Freely assignable voice engines
Each of the 6 voice slots can run any engine: drum, snare, cymbal or hi-hat (select a voice, then
SHIFT + KIT, or NRPN 98). The engines are saved in the kit; stock kits load with the stock engines.

Details for kits, patterns and engines: [docs/ENGINES.md](docs/ENGINES.md).

### Expanded sequencer
- **Step arp:** the flam substeps of a step can play a chord (Maj, Min, Dim, Aug and their sevenths) up
  or down, set per step with `chd` and `dir` next to flam and shift.
- **Song mode:** the song page shows the pattern names, the cursor and the playing slot, and
  `REP x01` to `REP x32` repeat the previous pattern before the song continues.

### New performance features
- **DJ filter** on the final mix, on the performance page: `dj` left of the center is a low-pass, right
  of it a high-pass, `res` sets the resonance.
- **LFOs on the whole unit:** an LFO can modulate the global sample rate or the DJ filter (target voice
  `Prf`).

### Improved workflow
- **Faster knobs:** hold SHIFT while turning a knob for bigger steps.
- **Note names** follow the MIDI convention (note 0 is C-1, 60 is C4).
- **Uninterrupted playback:** knobs, menus, loading and saving don't interrupt the audio or the
  sequencer.

### Misc.
- **Limiter:** voices and FX are summed in 32 bit and limited per output, so loud kits no longer
  hard-clip. Config menu "lim" switches it off for exactly the stock mix.
- **Bug fixes:** many stock firmware bugs fixed (MIDI, sequencer, SD card, memory corruption; most found
  in the firmware review by jhw in
  [erica-synths/lxr-desktop-app#3](https://github.com/erica-synths/lxr-desktop-app/issues/3)), a project
  save that can't lose the saved project, and about 10 % less CPU load. Stock issues kept on purpose:
  [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

The changes of each version are in the [release notes](https://github.com/tunefish777/openelixir-02/releases).

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
