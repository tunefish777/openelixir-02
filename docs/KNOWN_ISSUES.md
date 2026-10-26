# Known issues

Issues of the stock LXR-02 firmware that OpenElixir-02 knowingly keeps, or that are still open. The
IDs refer to the firmware review by jhw in
[erica-synths/lxr-desktop-app#3](https://github.com/erica-synths/lxr-desktop-app/issues/3).

## Kept on purpose (stock sound)

- **D13: wavetable interpolation reads past the end of a table row.** The oscillators interpolate
  between `table[row][i]` and `table[row][i+1]`, and for the last sample of a row that is the first
  sample of the next row (and past the last table for the top octave, MIDI notes from 120). This adds
  small clicks/spikes, mostly at very high notes and on some rectangle rows. A fix (wrapping to
  `(i+1) & 1023`) would change the sound of existing kits, so it is not applied. It only reads; no
  memory is overwritten.

- **S-B6: internal shuffle also shuffles the MIDI and analog clock outputs.** The clock outputs come
  from the same shuffled 96 ppq ticks as the steps, so a device synced to the clock outputs follows
  the shuffle. Kept as in the stock firmware (decision 2026-10-26).

## Known limitations

- **P7: pulling the SD card during an access hangs the unit.** The SD driver waits without a timeout.
  Not planned to be fixed: don't remove the card while it is being read or written.
