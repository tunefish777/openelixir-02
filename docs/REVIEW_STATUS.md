# OpenElixir-02 v1.2 vs. the stock firmware review

How each finding from the firmware review in
[erica-synths/lxr-desktop-app#3](https://github.com/erica-synths/lxr-desktop-app/issues/3)
(stock v1.7, `a43cd4f`) stands in OpenElixir-02 v1.2 (`383b012`). Also covered: the follow-up
comment in #3 and the three findings in
[erica-synths/lxr-desktop-app#7](https://github.com/erica-synths/lxr-desktop-app/issues/7).

Built 2026-11-01 (with the help of Claude, an AI coding assistant) by matching the review against
the OpenElixir commit messages (most of them cite review IDs) and then checking the cited code in
both trees. Nothing here was tested on hardware. Commit hashes refer to this repository; line
numbers refer to v1.2 unless the row says "stock".

**Status**
- **Fixed**: a commit cites the ID or describes the fix, and the code change is there.
- **Partly**: part of the finding is fixed, the rest isn't.
- **Kept**: left as in stock on purpose ([KNOWN_ISSUES.md](KNOWN_ISSUES.md)).
- **Open (checked)**: the code at the cited location still has the problem, or the file is unchanged.
- **Open?**: no commit mentions it and no fix was found, but the code wasn't traced end to end.

## Summary

| Status | Count |
|---|---|
| Fixed | 35 |
| Partly | 8 |
| Kept | 3 |
| Open (checked) | 19 |
| Open? | 15 |

A few IDs were checked in groups (e.g. M4/M5/S-B9, S-C2..C6, U-B10..12), so counts are by table row.

## Memory corruption / out of bounds

| ID | Finding | Status | Evidence |
|---|---|---|---|
| M1 | External NRPN ≥128 writes past `midiParser_originalCcValues` | Fixed | `f600975` |
| P1 | Project save into existing folder corrupts kit 1's name | Fixed | `5e99951`, `ff93732` |
| S-A1 | Song starting with LOOP plays "pattern 65" | Fixed | `14a127f` |
| K-3 | Song bytes above 65 index past the patterns | Partly | Not cited. The new `seq_pendingPattern >= MAX_PATTERN` guard in `seq_nextStep` (`14a127f`) catches them when they become pending. The other read sites (`sequencer.c:565-602`) weren't checked. |
| S-A2 | `seq_stepIndex` -1 while stopped (record, roll, flam) | Fixed | `f600975` |
| U-A1 | SHIFT + held select arms step up to 127 (v1 substep formula) | Fixed | `f600975` |
| U-A2 | Menu value names read with no bounds check; waveform text overruns buffer | Fixed | `f600975` |
| M13 | NRPN FX type reads past `menuPages` | Fixed | `f600975` |
| P8 | Unchecked index bytes from `tmp.snd` / `proj.idx` | Fixed | `ff93732` |
| K-1b | CC 0 kit index 64-127 reads past `mainParameters` | Fixed | `14a127f` |
| K-2 | Track length and kit number from pattern files not validated | Fixed | `ff93732` |
| M2 | USB-MIDI TX ring wrap sends 128 bytes from past the buffer | Fixed | `f600975` (`usb_manager.c` rewritten send path) |
| M12 | `CDC_Transmit_FS` dereferences NULL before enumeration | Fixed | `f600975` |
| D5 | RAT distortion reads `table_1n914[257]` | Fixed | `f600975` |
| D13 | Wavetable interpolation reads one past a table row | Kept | KNOWN_ISSUES: fixing it changes the sound |
| U-D | `UNUSED_NAMES` larger than `NUM_SAVEFILE_NAMES` | Fixed | `f600975` |

## Data loss (SD card)

| ID | Finding | Status | Evidence |
|---|---|---|---|
| P2 | Project save empties the folder first; a failed save loses both copies | Fixed | `ff93732` (writes `/projNNn`, then swaps) |
| P3 | `f_write`/`f_close` results ignored, full card reports "Save OK" | Fixed | `ff93732` |
| P4 | Rename leaves the old file in `/tmp`, loaded after reboot | Fixed | `ff93732` |
| P5 / U-B6 | Save, reload and clear kit use the load/save page cursor | Fixed | `ff93732`, `buttonHandler.c` now uses `menu_loadedPresetNr` |
| P6 | Early return leaks the only FatFs lock, SD dead until reboot | Fixed | `ff93732` |
| P7 | SD driver busy-waits with no timeout; pulling the card hangs the unit | Kept | KNOWN_ISSUES: "not planned"; `sd_diskio.c` unchanged |
| P10 | Failed loads leave mixed state | Fixed | `ff93732` |
| P11 | Short `glo.cfg` rejected, defaults written into `/proj00` | Fixed | `ff93732`, `main.c` |
| P9 | Init kit copied with an off-by-8 offset | Fixed | `ff93732` |

## MIDI

| ID | Finding | Status | Evidence |
|---|---|---|---|
| K-1 | CC 0 bank select half-implemented (name, bounds, state, recorded as automation) | Fixed | `14a127f` |
| M6 / S-B2 | Every sequencer hit sends Note On twice; hanging notes | Fixed | `14a127f` |
| M7 | Per-voice CC channel filter off by one voice | Fixed | `14a127f` |
| M4/M5/S-B9 | Realtime bytes mid-message corrupt it; MIDI thru sends `F8 3C 64`; F3 / system-common running status | Partly | `14a127f` fixes realtime bytes and thru. Neither the commit nor the README mentions F3 song select or system-common running status. |
| M8 | NRPN mute 1000-1006 only updates LEDs, 200-206 only mutes the engine | Partly | `f600975` stops 200-206 corrupting memory and leaves 1000+ to the front panel. Whether both now mute the engine *and* the LEDs wasn't checked. |
| M9 | MIDI→USB forwarding builds wrong CINs for SysEx, MTC, SPP, song select | Open (checked) | The `usb_manager.c` diff only touches the send path (M2), not the forwarding code at lines 136-146 |
| M10 | USB-MIDI RX queue has no overflow check; main loop drains one message per pass | Open (checked) | `usbd_cdc.c` unchanged |
| M11 | UART TX FIFO drops single bytes when full, leaving partial messages | Open (checked) | `Uart.c` unchanged |
| S-B7 | Song end doesn't send MIDI Stop; front panel still thinks it's running | Fixed | `14a127f` |
| S-B8 | Continue resets to step 0; shuffle state not cleared; no SPP | Open (checked) | Not cited; `clockSync.c` unchanged |
| K-4 | Global MIDI channel 0 sent as 0xFF | Open? | Not cited. Stock `menu.c` still computes `value-1` there; not traced. |
| S-C1 | `seq_setMute` passes a MIDI channel where a voice number is expected | Open? | Not cited |
| M-detune | `midiParser_calcDetune`: both branches identical, detune-down uses `SEMITONE_UP` | Open (checked) | Same code in `MidiParser.c:113-127` |

## Concurrency

| ID | Finding | Status | Evidence |
|---|---|---|---|
| M3 / S-B10 / D19 | MIDI parsed from both TIM2 ISR and main loop; non-volatile shared counters | Open? | Not cited; `Uart.c` and `TriggerOut.c` unchanged |
| U-C2 | UI work (repaint, LEDs) runs from TIM14 (priority 0), TIM2 and main loop | Partly | `d91abf5` moves page timers and full repaints out of TIM14 into the main loop; non-volatile flags not mentioned |
| D15 | Audio DMA at lowest priority, restarted by hand every block | Fixed | `d91abf5`: circular DMA, priority 1 |

## Sequencer timing / behaviour

| ID | Finding | Status | Evidence |
|---|---|---|---|
| S-B1 | Euclid generator stores length 16 as 0 | Open (checked) | `EuklidGenerator.c` unchanged (`if(len == 16) len=0;`) |
| S-B3 | `seq_triggerVoice` uses `seq_stepIndex`, not the shifted/flammed step | Open (checked) | `14a127f` only adds the `step >= 0` guard |
| S-B4 | `chokeCurrent`/`chokeNext` shared by all tracks | Open? | Not cited |
| S-B5 | Integer `60000/bpm` (128 bpm plays at 128.205); TIM14 PSC 13500 | Partly | `14a127f` makes the division float; the PSC question isn't mentioned |
| S-B6 | Internal shuffle also swings MIDI/analog clock out | Kept | KNOWN_ISSUES, decision 2026-10-26 |
| S-C2..C6 | Quantised notes play twice; probability on spilled flams; length change during playback; `seq_deleteRow` duplicate; stale Euclid display | Open? | Not cited |

## User interface

| ID | Finding | Status | Evidence |
|---|---|---|---|
| U-B1 | SHIFT+Config makes next step press set all track lengths | Fixed | `14a127f` |
| U-B2 | Parameter-lock slots never freed; later locks wipe voice 1/2 automation | Fixed | `14a127f` |
| U-B3 | Lock preview reads track 0, off by one ≥128 | Fixed | `14a127f` |
| U-B4 | Voice 7 LFO target remap reads `PAR_RETRIGGER_LFO1` | Open? | Not cited |
| U-B5 | Turning a pot over an empty column records `PAR_NONE` automation | Open? | Not cited |
| U-B7 | Releasing a step with Shift/Load held leaves its long-press timer running | Open? | Not cited; no matching change in the `buttonHandler.c` diff |
| U-B8 | Copy + Page Up (duplicate bar) doesn't copy flam/shift | Open? | Not cited; no matching change in the `buttonHandler.c` diff |
| U-B9 | `led_clearVoiceLeds` mask is `0x80`, should be `0x01` | Open (checked) | `ledHandler.c:129` still `&= 0x80` |
| U-B10..12 | Note encoder stops at 100, notes ≥120 show ':'; new save name "Empty"; Clear menu text cut off | Open? | Not cited |
| U-C1 | Front-panel buttons have no debounce | Open (checked) | `IO/DIO.c` unchanged |

## DSP / audio

| ID | Finding | Status | Evidence |
|---|---|---|---|
| D1 | Pitch-EG slope 127 → NaN, oscillator stops | Fixed | `8ad5946` |
| D2 | Snare 16-bit conversion wraps | Fixed | `8ad5946` |
| D3 | Ring mod in mono routing adds the existing output into `wet` | Open (checked) | `Ringmod.c:73` still adds `*outL` into `wet` (only ported to 32 bit) |
| D4 | Mono compressor has no saturation | Fixed | `8ad5946` |
| D8 | Hat/cymbal `pitchMod` stays detuned when transient leaves snap | Open? | Not cited; `HiHat.c:153` still sets it only in the snap branch, reset path not traced |
| D10 | Boot click from debug value `dma_buffer[0]=32756`; DAC start buffers swapped | Partly | `d91abf5` fixes the swapped buffers; `AudioCodecManager.c:26` still writes 32756 |
| D11 | Mono FX routing drops the right channel of the FX bus | Open (checked) | `FxBus.c` now *documents* it ("mono out: left channel of the FX bus"); may be deliberate |
| D12 | Ping-pong delay halves overlap by one sample | Open? | Not cited |
| D6, D7 | PWM phase saturates instead of wrapping; FM negative half clipped | Open (checked) | `Oscillator.c` unchanged. Sound-changing, so it would need a config switch under the project rules. |
| D9 | Attack 127 gives increment 0, voice never sounds | Open (checked) | `SlopeEg2.c` unchanged (review: "possibly intentional") |

## Platform / performance

| ID | Finding | Status | Evidence |
|---|---|---|---|
| D14 | ADC DMA HT/TC interrupt ~128k/s with no callback | Open? | Not cited. The v1.1 CPU meter measures "ADC interrupt load", so the author is aware of it. |
| D16 | Flush-to-zero set via `FPDSCR` only | Open (checked) | Same single `FPDSCR` site as stock |
| D17 | NaN guard in `ResonantFilter` folded away by `-ffast-math` | Open (checked) | `ResonantFilter.c` unchanged |
| D18 | Noise uses blocking `HAL_RNG_GetRandomNumber` per sample | Open (checked) | `random.c` unchanged |
| D20 | ITCM/DTCM load images not counted in FLASH; 1 KB min stack | Partly | `bf9efc7` fixes the FLASH accounting; stack size not mentioned |
| D21 | int16 buffers type-punned as `int32_t*` | Partly | The 32-bit mix bus (`7501ff3`) removes many casts in FX/mixer; `BufferTools.h` unchanged |
| — | `double` math in `OnePoleFilter`, `bufferTool_addGainInterpolated`, `Compressor` | Open? | Not cited; `OnePoleFilter.h` unchanged (it is inlined in voices since `35f5510`) |
| — | v1 dead code cleanup | Open? | Not cited as such (`MidiParser.c` lost ~190 lines net, not checked against the list) |

## Follow-ups outside the main list

| Source | Finding | Status | Evidence |
|---|---|---|---|
| #3 comment | Ping-pong delay indexes past `FX_BUFFER` after a type switch | Fixed | `796e9e2` |
| #3 comment | Front-panel parser uses 7-bit values as step/track/pattern indices | Fixed | `f600975` ("front panel messages used unchecked step, track and pattern numbers") |
| #7 (1) | SD format saves project 0 before `seq_init()`, every track length 0 | Open (checked) | `main.c:175` `preset_saveProject(0)` still runs before `soundengine_init()` at line 474 |
| #7 (2) | `modNode_setOriginalValueChanged` reads through a NULL `ptr` | Open (checked) | `modulationNode.c:50` still reads `*((uint8_t*)p->ptr)` with no NULL check |
| #7 (3) | `CodecInit()` declared without parameters, called with one | Open (checked) | `AudioCodecManager.h:23` unchanged |
