# LXR V2 firmware: build, update and development guide

This is the README of the original open-source LXR V2 firmware by Erica Synths, kept as the detailed
guide for building, installing and developing the firmware. File names have been updated for
OpenElixir-02. For an overview of the project, see [README.md](README.md).

> **Licence and trademarks:** the firmware source code is free software under the GNU GPL version 3
> ([LICENSE](LICENSE)). You're welcome to study, change and share it, for example to write your own
> updates for your LXR. The names **"Erica Synths" and "LXR"** and the **LXR hardware** are the property
> of Erica Synths and are **not** covered by this licence. Building and installing your own firmware is
> at your own risk: Erica Synths provides no support for self-built or modified firmware. See
> [Licence, trademarks and hardware](#licence-trademarks-and-hardware) below and [NOTICE.md](NOTICE.md).

## Updating your LXR

Every LXR has a built-in updater (the bootloader) that installs new firmware from the SD card. You
don't need any extra hardware:

1. [Build the firmware](#building-the-firmware). This creates the update file `LXR_OpenElixir-02_V<version>.img`
   (the LXR's updater only accepts files whose name starts with `LXR`).
2. [Copy it to the SD card and install it](#installing-the-update) on the LXR.

### Building the firmware

You need [CMake](https://cmake.org/download/) 3.21 or newer, [Ninja](https://ninja-build.org/),
the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-gcc`, tested with 14.3.rel1 and 11.3.rel1) and [Python 3](https://www.python.org/downloads/)
(to create the `.img` update file). The build finds them on `PATH`.

**1. Install the tools (once)**

*macOS* (Apple Silicon and Intel), with [Homebrew](https://brew.sh):

```sh
brew install cmake ninja
brew install --cask gcc-arm-embedded
```

Use exactly this `--cask gcc-arm-embedded` package, the official Arm toolchain. The similarly named
`brew install arm-none-eabi-gcc` does **not** include the C library the firmware needs, and the build
fails with it. Python 3 comes with the Xcode Command Line Tools, which Homebrew installs.
If you installed the toolchain with Arm's own `.pkg` installer instead, it isn't on `PATH`: add
`-DARM_TOOLCHAIN_DIR=/Applications/ArmGNUToolchain/<version>/arm-none-eabi/bin` to the first `cmake`
command below.

*Windows:* install CMake, Ninja and Python 3 (e.g. `winget install Kitware.CMake Ninja-build.Ninja
Python.Python.3.12`) and the Arm GNU Toolchain with its Windows installer. On the installer's last page,
tick "Add path to environment variable".

*Linux:* install CMake, Ninja and Python 3 with your package manager (e.g.
`sudo apt install cmake ninja-build python3`) and the Arm GNU Toolchain from Arm's download page, then
add its `bin` folder to `PATH`.

**2. Build**

Open a terminal (Terminal on macOS, PowerShell on Windows) in this folder and run:

```sh
cmake --preset release
cmake --build --preset release
```

When the build has finished it shows where the update file is and how to install it:

```
======================================================================
  Firmware update file:
    .../lxr_desktop_app/build/release/LXR_OpenElixir-02_V0.1.img
  ...
```

The version in the file name, and on the LXR's start-up screen, is set by `project(VERSION ...)` in
[CMakeLists.txt](CMakeLists.txt).

### Installing the update

1. Copy `LXR_OpenElixir-02_V<version>.img` from `build/release/` to the top folder of the SD card. Delete any
   older `LXR*.img` files from the card: the updater installs the first one it finds.
   On a Mac, eject the card in Finder before you take it out. Finder also leaves hidden files starting
   with `._` on the card; the updater ignores them.
2. Switch the LXR off and put the card in.
3. Hold down the encoder button and switch the LXR on. Keep holding the button until
   "LXR Updater" appears on the display.
4. The display shows "Checking Firmware...", then "Loading Firmware" with the progress in percent.
5. When it shows "Update OK!" the LXR restarts by itself with the new firmware.

The updater itself is never changed by an update. If an update fails or is interrupted (for example by
a power cut), the LXR can always be updated again the same way.

### Troubleshooting

If the updater stops with one of these messages, switch the LXR off, fix the cause and try again:

| Display | Cause |
|---|---|
| `can't mount` / `SD Card` | No SD card, or the card can't be read. The card must be formatted as FAT32. Cards larger than 32 GB usually come formatted as exFAT, which the updater can't read. On a Mac, reformat the card in Disk Utility with the format "MS-DOS (FAT)". |
| `LXRE*.img` / `Not Found!` | No update file in the top folder of the card. The file name must start with `LXR` and end with `.img`. |
| `Can't open` / `LXRE*.img!` | The update file couldn't be read. Copy it to the card again. |
| `LXR*.img` / `Not Valid!` | The file isn't an update for the desktop LXR (for example a Eurorack module update, or a damaged file). |
| `LXR*.img` / `Wrong Size!`, `Wrong` / `Checksum!` | The file is damaged or incomplete. Build it again or copy it to the card again. |
| `Can't erase` / `Flash!`, `Flash write` / `Error!` | Writing the firmware to the LXR's memory failed. Switch the LXR off and try the update again. |

## Build options

| Option | Effect |
|---|---|
| `cmake --preset debug` | Debug build: `-O3 -g3` with `DEBUG` defined, output in `build/debug/`. The release build uses `-O3 -g0`. Both need `-O3` for the audio engine to keep up in real time. |
| `-DARM_TOOLCHAIN_DIR=<bin folder>` | Use a toolchain that isn't on `PATH`, e.g. `/Applications/ArmGNUToolchain/14.3.rel1/arm-none-eabi/bin` (macOS) or `C:/toolchain_arm-none-eabi/11.3rel1/bin` (Windows) |
| `-DLXR_ITCM_OPT=<flag>` | Optimisation for the code in the fast ITCM memory, see [Memory map](#memory-map) (default `-O2`) |
| `-DLXR_GENERATE_LISTING=ON` | Also write a disassembly listing (`.list`) |
| `-DLXR_MEMORY_BASELINE=<file>` | Memory usage report to compare every build against, see [Memory usage report](#memory-usage-report) (default `memory_baseline.json`) |
| `-DLXR_CPU_METER=ON` | Development build with a CPU load display, see [CPU load meter](#cpu-load-meter) (default `OFF`) |

Options are passed when configuring, e.g. `cmake --preset release -DLXR_GENERATE_LISTING=ON`.
The toolchain file and the CMake modules and tools are in the `cmake/` folder.

All files the build creates (in `build/<preset>/`):

| File                     | Purpose                                                        |
|--------------------------|----------------------------------------------------------------|
| `LXR_OpenElixir-02_V<version>.img`  | **Update file for the SD card** (see [Installing the update](#installing-the-update)) |
| `OpenElixir-02.elf`   | Firmware with debug information, for a debugger               |
| `OpenElixir-02.bin`   | Raw firmware image (from 0x08008000)                          |
| `OpenElixir-02.hex`   | Intel HEX of the same                                         |
| `OpenElixir-02.map`   | Linker map                                                    |
| `OpenElixir-02.elf.memory*.json` | Memory usage of this and the previous build, for the "vs. last" column |

## Cleaning the build

**Remove the compiled files** (including the `.img`) but keep the settings. The next build then
compiles everything again:

```sh
cmake --build --preset release --target clean
```

Use `--preset debug` to clean the debug build.

**Start completely from scratch:** delete the build folder, which also forgets all settings such as
`-DARM_TOOLCHAIN_DIR` or `-DLXR_BOOTLOADER_HEX`, then configure and build again:

```sh
rm -rf build                        # Linux, macOS, Git Bash
Remove-Item -Recurse -Force build   # Windows PowerShell
rmdir /s /q build                   # Windows Command Prompt

cmake --preset release
cmake --build --preset release
```

Start from scratch when you switch to a different compiler (another `ARM_TOOLCHAIN_DIR`, or a
different `arm-none-eabi-gcc` on `PATH`), because an existing build folder keeps using the compiler it
was first set up with. Do the same if the build behaves strangely after updating the sources.
With CMake 3.24 or newer, `cmake --preset release --fresh` redoes the setup without deleting the folder.

The `build/` folder is never committed to git.

## Memory map

| Region   | Address                   | Size   | Contents                                              |
|----------|---------------------------|--------|-------------------------------------------------------|
| FLASH    | 0x08000000 – 0x08007FFF   | 32 KB  | Bootloader (`LXR_V2_STM32F765_Bootloader`)            |
| FLASH    | 0x08008000 –              | 992 KB | This firmware (`VECT_TAB_OFFSET` 0x8000)              |
| ITCM     | 0x00000000                | 16 KB  | `IN_ITCM_RAM` hot DSP code, copied there by `main()`  |
| DTCM     | 0x20000000                | 128 KB | `INDTCM`/`INDTCMZ` data, DMA buffers, the stack       |
| RAM      | 0x20020000                | 384 KB | `.data`, `.bss`, heap                                 |

The ITCM is tight. Current GCC releases generate too much code at `-O3` for it, so the translation
units that contain ITCM code are compiled with `LXR_ITCM_OPT` (default `-O2`). Watch the ITCM line
of the [memory usage report](#memory-usage-report) when changing or adding `IN_ITCM_RAM` code.

The initial values of `.data`, `.dtcm` and `.itcm_text` are stored in FLASH behind `.rodata`
(`AT> FLASH` in `STM32F765VGTX_FLASH.ld`), so they count towards, and are checked against, the
FLASH region. The stack starts at the top of the DTCM and grows down: the free DTCM is the stack space.

### Memory usage report

Every build prints the usage of each memory region (`cmake/tools/memreport.py`):

```
Region          Used       Size       Free    Use                          vs. last  vs. base
---------------------------------------------------------------------------------------------
ITCM_RAM      15,472     16,384        912  94.4%  [###################.]!      +336      +336
  .itcm_text                       15,472
  (of which linker veneers)           472  (59 long-branch stubs)
DTCM         107,392    131,072     23,680  81.9%  [################....]          0         0
  ...
  stack (the free space)           23,680  (linker minimum 1,024)
...
```

- **Used** is counted like the linker's `--print-memory-usage` (alignment gaps included); FLASH
  includes the initial values of the RAM and TCM sections, i.e. it equals the `.bin` size.
- **vs. last**: change since the previous build of this build folder.
- **vs. base**: change since the saved baseline `memory_baseline.json` (stock firmware 1.7 built
  with Arm GNU Toolchain 15.3.Rel2). Other compiler versions give somewhat different sizes; the
  report says so when the compilers differ.
- `!` marks regions that are at least 90 % full.

Targets:

```sh
cmake --build --preset release --target memory_report     # also the 15 largest symbols per region, vs. baseline
cmake --build --preset release --target memory_baseline   # save the current build as the new baseline
```

### CPU load meter

A development build that shows on the display how much of the available CPU time the audio uses.
Build it in its own folder so it doesn't replace your normal build:

```sh
cmake --preset release -B build/cpumeter -DLXR_CPU_METER=ON
cmake --build build/cpumeter
```

This creates `build/cpumeter/LXR_<name>_V<version>_cpumeter.img`; install it like a normal update.
Normal builds don't contain any of it (the code is in `#if CPU_METER`, see
`Core/Inc/Soundengine/CpuMeter.h`), and CI builds the meter variant too so that it keeps compiling.

On the device, SHIFT + PROJECT steps through: off, load page, section page, timing page, stall page.
Switching pages keeps the measurements. Load and section values are in percent of one audio block (64 samples, about 1.45 ms); "pk"
values are peaks since the last reset.

| Page | Shows |
|---|---|
| Load: `dsp 78% pk 80%` | Rendering of all voices and the FX (`mixer_calcNextSampleBlock`), current and peak |
| Load: `all pk 82% u0 +0` | Peak of all audio work per block (sequencer, rendering, MIDI): what the UI can't use. `u` = buffer underruns (dropouts) since the reset, `+N` = simulated extra voices |
| Sections: `D12 12 12 S10` | Peaks of drum voices 1-3 and the snare |
| Sections: `C10 H10 fx12 o11` | Peaks of cymbal, hi-hat, FX bus and the rest of the mixer (LFOs, envelopes, sample-rate reduction, panning/summing, limiter) |
| Timing: `dma  12us lt 30%` | `dma`: longest delay of an I2S DMA callback (interrupt latency; the DMA runs in circular mode, so only a delay of more than a block matters). `lt`: longest wait from a block request until its rendering starts, in percent of a block; `lt` plus the render time above 100 % is a dropout. It can't show more than one missed block: count those with `u` |
| Timing: `adc12% seq 0.3ms` | `adc`: CPU share of the fader/knob ADC interrupt (current). `seq`: longest delay of an internal sequencer clock tick (96 ppq) while running; up to 0.25 ms is the clock's resolution |
| Stalls: `stall  27.6ms` | Longest time without rendering a block (only counted above two blocks) |
| Stalls: `pc 08012345 blk` | Code address that was running during that stall; find the function with `arm-none-eabi-addr2line -f -e build/cpumeter/OpenElixir-02.elf 0x08012345`. `blk`: the measuring interrupt itself was held off (interrupts disabled, or an interrupt of the highest priority running) |

SHIFT + MORPH renders the cymbal engine 0 to 4 extra times per block (`+N`) to simulate kits with more
FM voices, and resets the peaks and the underrun count. On the timing and stall pages it only resets.
The extra renders use a separate cymbal voice and don't change the sound. Measured with a worst-case kit on the stock voice layout: `dsp` peak 78 %,
`all` peak 82 %, drum voice 12 % each, snare/cymbal/hi-hat 10 %, FX 12 %, rest 11 %.

## Source layout and CMake targets

The project was originally generated with STM32CubeMX/STM32CubeIDE, which explains the directory
layout and the `USER CODE BEGIN/END` comments. It no longer depends on either tool: the CubeMX project
(`.ioc`) was removed because regenerating code from it overwrites application code in `main.c`.
Pins, clocks and peripherals are configured in `Core/Inc/main.h`, `Core/Src/main.c`
(`SystemClock_Config`, `MX_*_Init`) and `Core/Src/stm32f7xx_hal_msp.c`; edit them there.

Every component is an OBJECT library linked into `OpenElixir-02`:

| Target          | Sources                                         | Contents                                              |
|-----------------|-------------------------------------------------|-------------------------------------------------------|
| `stm32f7_hal`   | `Drivers/`                                      | ST HAL + CMSIS                                         |
| `fatfs`         | `Middlewares/Third_Party/FatFs`, `FATFS/`       | FatFs and the SD card glue                            |
| `usb_midi`      | `Middlewares/JS`, `USB_DEVICE_MIDI/`            | USB device stack with the custom USB-MIDI class       |
| `lxr_platform`  | `Core/Startup`, `Core/Src/main.c`, ...          | Startup, clocks, peripheral init, interrupts          |
| `lxr_io`        | `Core/Src/IO`                                   | Button/LED shift registers, pots and faders           |
| `lxr_ui`        | `Core/Src/menu`, `lcd.c`, `buttonHandler.c`, ...| LCD, encoder, buttons, LEDs, menu pages               |
| `lxr_preset`    | `Core/Src/Preset`                               | Kit/pattern/project load & save on SD                 |
| `lxr_dsp`       | `Core/Src/Soundengine/DSPAudio`                 | Oscillators, filters, envelopes, voices, master FX    |
| `lxr_engine`    | `Core/Src/Soundengine/*.c`                      | Codec/I2S DMA, sample block rendering, FX bus, clock in/out |
| `lxr_midi`      | `Core/Src/Soundengine/MIDI`                     | MIDI UART, parsers, parameter mapping                 |
| `lxr_sequencer` | `Core/Src/Soundengine/Sequencer`                | Step sequencer, Euclidean generator, clock sync       |
| `lxr_usb_app`   | `Core/Src/Soundengine/USB`                      | USB-MIDI message queue                                |

Source files are listed explicitly in the `CMakeLists.txt` files. Add new files there.

## Advanced: programming with a debug probe

Not needed for updates: use the [SD card](#installing-the-update) for that. This section is for
development, factory programming and repairs, with an ST-Link debug probe connected to the LXR's
debug header and [OpenOCD](https://openocd.org/) installed (the build finds it on `PATH`; on macOS:
`brew install open-ocd`).

**Firmware only.** Writes just the firmware; the bootloader is left alone:

```sh
cmake --build --preset release --target flash
```

**Blank devices (bootloader + firmware).** A blank device, or replacing the bootloader on purpose,
needs the bootloader as well. The bootloader is a separate project, so by default the build doesn't
include it. Tell the build where to find it in one of two ways:

```sh
# A: a prebuilt bootloader .hex (e.g. from a bootloader release)
cmake --preset release -DLXR_BOOTLOADER_HEX=path/to/LXR_V2_STM32F765_Bootloader.hex

# B: the bootloader source code, which is then built together with the firmware
cmake --preset release -DLXR_BOOTLOADER_DIR=path/to/LXR_V2_STM32F765_Bootloader
```

The build checks that the bootloader is valid for this device: it must fit in the 32 KB bootloader
sector and accept desktop (`LXRV2IMG`) update images, so the firmware, a full image or the Eurorack
bootloader are rejected. It then also writes `OpenElixir-02_V<version>_full.hex`/`.bin` (bootloader +
firmware, the whole flash from 0x08000000) and enables:

```sh
cmake --build --preset release --target flash_full
```

The same `.hex`/`.bin` can also be loaded with STM32CubeProgrammer (the `.bin` goes to 0x08000000).
To go back to a firmware-only build, clear the setting: `-DLXR_BOOTLOADER_HEX=` or `-DLXR_BOOTLOADER_DIR=`.

> **Warning:** `flash_full` overwrites the bootloader. If that is interrupted, the device can no longer
> update itself from the SD card and has to be recovered with a debug probe. The target therefore
> prints a warning and only continues after you type `yes`; without an interactive terminal (e.g. a
> build started from an IDE) it refuses. For scripts and factory programming, set the environment
> variable `LXR_CONFIRM_BOOTLOADER_OVERWRITE=yes` to skip the question.

The bootloader write-protects its own flash sector (sector 0) every time it boots, so on a unit that
has run before, sector 0 cannot simply be erased. `flash_full` therefore clears that protection first;
the new bootloader sets it again on its first boot. Other programming tools need the same step
(in STM32CubeProgrammer: Option bytes → Write protection → uncheck nWRP0).

The OpenOCD settings can be changed with `LXR_OPENOCD_INTERFACE`, `LXR_OPENOCD_TRANSPORT`,
`LXR_OPENOCD_SPEED` (default 1000 kHz) and `LXR_OPENOCD_WORKAREA` (default 0x8000). The defaults are
deliberately conservative: with a 64 KB work area at the faster default clock, an ST-Link V2 (firmware
V2J29) failed half-way through programming.

## Licence, trademarks and hardware

Copyright (C) 2012-2026 Erica Synths.

**Firmware licence.** The firmware, the bootloader and the build system are licensed under the
[GNU General Public License version 3](LICENSE) (GPL-3.0-only), with an additional permission for
the STMicroelectronics libraries and the additional terms in [NOTICE.md](NOTICE.md). In short:

- You may use, study and change the firmware, for example to build your own updates for your device.
- If you share or distribute the firmware, or a modified version of it, including pre-installed on a
  device, you must provide the complete source code under the same licence, and the people you give it
  to may install their own modified versions.
- Modified firmware must be marked as modified (its start-up screen must show this, e.g. with a
  different name or version text) and must not be presented as official Erica Synths firmware.
- The firmware comes **without any warranty**. Building and installing your own or modified firmware is
  entirely at your own risk, and Erica Synths provides no support for it.

**Trademarks.** "Erica Synths", "LXR" and "LXR-02" and the associated logos are trademarks of
Erica Synths. The GPL licence does **not** grant any right to use them. They may not be used for any
product, firmware release, service or commercial activity without prior written permission from
Erica Synths. Truthful references such as "custom firmware for the Erica Synths LXR-02" are fine.

**Hardware.** The LXR hardware (schematics, circuit board layouts, front panel, enclosure and industrial
design) is the intellectual property of Erica Synths and is protected by copyright and design rights.
It is **not** part of this repository and is **not** licensed under the GPL or any other licence. It may
not be copied, manufactured or sold, in whole or in part, without prior written permission from
Erica Synths.

**Third-party code.** The repository includes components from STMicroelectronics, Arm and ChaN (FatFs)
under their own licences. See [NOTICE.md](NOTICE.md#third-party-components).


<sup>
Use code OPENLXR15 on the Erica Synths website for a discount on the LXR (active for 1 week since open-source announcment) :)
</sup>