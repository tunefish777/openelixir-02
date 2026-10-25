# Voice engines

Each of the 6 voice slots can run any of the 4 engines: drum, snare, cymbal or hi-hat. Track 7
is always the second trigger of slot 6, for any engine: track 6 plays with decay A, track 7 with
decay B, and each cuts the other off (like closed and open hi-hat).

## Changing an engine

On the device: select a voice and press SHIFT + KIT (step button 8) to open the engine menu (like the
load/save menu, it stays open and the step buttons don't show the voice's steps). Turn the encoder to
choose the engine and press the encoder button to apply it. The voice buttons select the voice; a
select button (OSC, AEG, ...) opens that page of the voice, any mode button another menu. Voice 6 and 7 share slot 6.

Via MIDI: NRPN 98 (CC 99 = 0, CC 98 = 98, CC 6 = value), value = slot × 4 + engine, with slots
0-5 and engines 0 drum, 1 snare, 2 cymbal, 3 hi-hat. Example for slot 4 → drum: `B0 63 00 B0 62 62
B0 06 0C`. Only a received NRPN changes the engine; it is not a kit parameter.

When a slot changes its engine, all its sound parameters (including volume, pan, output and LFO)
start from the new engine's defaults (the init kit), its own LFO and velocity targets are turned
off, and modulation and step automation of its engine specific settings are turned off.

The menu pages, value ranges and modulation target names of a slot follow its engine.

## Kits

The engines are saved in kits, projects and the autosave (a block after the 247 parameter bytes of
the `.snd` file). Kits without it, from the stock firmware or anywhere else, load with the stock
engines and sound as before. The stock firmware loads OpenElixir-02 kits too, but plays every slot
with its stock engine.

Morph keeps the engines of the source kit; the engine specific settings of a slot whose engine
differs in the morph kit are not morphed.

## MIDI CCs

All CC and NRPN numbers are unchanged from the stock firmware. Parameters every engine has (pitch,
filter, envelope, volume, pan, LFO, transient, ...) keep their meaning. Slot 6 decay A is CC 61,
decay B CC 62, for any engine.

Each slot has 7 engine specific controls. Analogous controls of the engines share a role, so a
controller mapping stays meaningful when the engine changes:

| Role | Drum | Snare | Cymbal | Hi-hat |
|---|---|---|---|---|
| 0 | mod osc wave | – | mod osc 1 wave | mod osc 1 wave |
| 1 | pitch EG decay | pitch EG decay | mod osc 2 wave | mod osc 2 wave |
| 2 | pitch EG amount | pitch EG amount | mod osc 2 freq | mod osc 2 freq |
| 3 | pitch EG slope | pitch EG slope | mod osc 2 gain | mod osc 2 gain |
| 4 | FM amount | tone/noise mix | mod osc 1 gain | mod osc 1 gain |
| 5 | FM freq | noise freq | mod osc 1 freq | mod osc 1 freq |
| 6 | mix/FM mode | repeat | repeat | – |

The role's CC or NRPN per slot (for the slot's stock engine these are its stock controls):

| Role | Slot 1 | Slot 2 | Slot 3 | Slot 4 | Slot 5 | Slot 6 |
|---|---|---|---|---|---|---|
| 0 | CC 21 | CC 22 | CC 23 | NRPN 95 | CC 24 | CC 26 |
| 1 | CC 71 | CC 72 | CC 73 | CC 74 | CC 25 | CC 27 |
| 2 | CC 75 | CC 76 | CC 77 | CC 78 | CC 31 | CC 35 |
| 3 | CC 79 | CC 80 | CC 81 | CC 82 | CC 33 | CC 37 |
| 4 | CC 83 | CC 85 | CC 87 | CC 29 | CC 32 | CC 36 |
| 5 | CC 84 | CC 86 | CC 88 | CC 28 | CC 30 | CC 34 |
| 6 | NRPN 6 | NRPN 7 | NRPN 8 | CC 69 | CC 70 | NRPN 96 |

NRPN 95 and 96 (formerly unused) are only needed when slot 4 runs a drum, cymbal or hi-hat (role 0)
or slot 6 runs a drum, snare or cymbal (role 6); they can't be modulated.
