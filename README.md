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

**Kit and pattern management.** Load full kits, single voices and patterns from any project:
- Hold LOAD and press step button 1-7 (Drum 1 ... OP HH) to load just that voice from any kit of
  any project into the current kit. It becomes part of the current kit, like any other change to it.
- "Change Project" below kit 0 in the load and save kit menus loads kits from other projects and
  saves kits into them. A kit loaded from another project replaces the current kit temporarily:
  nothing is written until you save it into a kit slot. A project save keeps the kit slot you had
  loaded before (unsaved changes of a kit from the own project are written into its slot, as in the
  stock firmware), the autosave leaves it out, so it isn't back after switching off, and loading
  another kit or a pattern change that loads its kit discards it.
- The pattern load and save pages (LOAD/SAVE + PATTERN) show the slot under the encoder as
  `[05] Name`. The encoder scrolls through all 64 slots and its button works like the step button
  of that slot. Lit step buttons show the slots with steps, the playing pattern blinks slowly and
  the next one fast. Saving asks for a name like kits do, and saving over another slot with steps
  asks for a second press first.
- "Change Project" above slot 1 on the pattern pages lists the patterns of other projects. A pattern
  loaded from another project replaces the playing pattern at the next pattern change (at once while
  the sequencer is stopped); only the pattern comes over, the kit stays. It plays like unsaved
  changes of the playing slot and is temporary too: nothing is written until you save it into a
  slot, a project save keeps the slot's saved pattern, the autosave leaves it out, and switching to
  another pattern discards it. Saving into another project writes only that project's pattern file
  (pointing to its kit 1); the own project doesn't change.

**Freely assignable voice engines.** Each of the 6 voice slots can run any engine: drum, snare,
cymbal or hi-hat. Select a voice and press SHIFT + KIT (or send NRPN 98). The engines are saved in
the kit; kits from the stock firmware load with the stock engines.

Details for kits, patterns and engines: [docs/ENGINES.md](docs/ENGINES.md).

**Limiter.** Voices and FX are summed in 32 bit, followed by a limiter on each output, so loud kits
no longer hard-clip. Levels that didn't clip before are unchanged. Config menu "lim" (2nd page)
switches it off for exactly the stock mix.

**DJ filter.** A filter on the final mix of all outputs, on the performance page: `dj` turned left of
the center is a low-pass (down to 40 Hz), right of it a high-pass (up to 8 kHz), around the center it
is off; `res` sets the resonance. It isn't saved and starts off; while off, the sound is unchanged.

**Uninterrupted playback.** Turning knobs, working in the menus, loading and saving kits and
patterns (also from and to other projects on the SD card) and saving projects don't interrupt the
audio or the sequencer while it plays.

**New in 1.4:**
- **Song page:** each row shows the slot (1-64) and the pattern's name, `>` at the cursor and `*` at
  the slot the song is playing. While choosing a slot's pattern, the number on the left shows the
  pattern (`p03`). SHIFT + PLAY always starts the song at slot 1.
- **Repeats in songs:** after LOOP, the entries `REP x01` to `REP x32` play the pattern before them that
  many more times (`REP x03`: 4 times in total), then the song continues. LOOP still jumps back to
  slot 1. The stock firmware ends a song at a REP entry.
- **Pattern names** are shown only if they were given in the pattern save dialog, otherwise "Pat 05".
  Pattern files from the stock firmware often contain leftover text in their name field.

**New in 1.3:**
- **Step arp:** two new step parameters next to flam and shift. `chd` picks a chord (Rep, Maj, Min,
  Dim, Aug, Maj7, Min7, Dim7, Aug7), `dir` the direction (Up, Dn), and the flam setting the number
  of notes. Rep is the stock flam. The arp is saved with the pattern; the stock firmware plays such
  steps as plain flams.
- **Faster knobs:** hold SHIFT while turning a knob to change values in bigger steps. Bipolar values
  stop at their center on the way.
- **LFOs on the whole unit:** set an LFO's target voice to `Prf` to modulate the global sample rate or
  the DJ filter cutoff and resonance. In the stock firmware such an LFO is off.
- **Pattern pages:** slot list with names on the encoder, lit slots with steps, slow/fast blinking
  playing and next pattern, a name dialog and an overwrite question when saving, and "Change
  Project" to load patterns from other projects and save into them (see Kit and pattern management).
- **Kits from other projects** stay temporary until saved into a slot; before, a project save or the
  autosave could write them into the kit slot loaded before.
- **Note names** follow the usual convention: MIDI note 0 is C-1, 60 is C4, 127 is G9.
- **Fixes:** a voice no longer stays silent until reboot after its filter blew up (very high
  cutoff, resonance and drive), turning one knob no longer nudges the value of another, and the
  encoder no longer needs an extra click when changing direction.

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
