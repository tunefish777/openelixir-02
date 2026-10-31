/*
 * DjFilter.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * Global DJ-style filter on the final mix (all outputs), set from the performance
 * page: one bipolar knob, below the center a low-pass closing towards 40 Hz, above
 * it a high-pass opening towards 8 kHz, around the center off; a second knob sets
 * the resonance. Not saved. While off, the output is bit-identical to no filter.
 */

#ifndef DJFILTER_H_
#define DJFILTER_H_

#include <stdint.h>

#define DJ_FILTER_OFF 63	/**< knob value in the center (shown as 0): filter off */
//knob values this close to the center are off too (the pots have no center detent)
#define DJ_FILTER_DEAD_ZONE	3

/** filter knob 0..127 (63 = off) and resonance 0..127, from the menu */
extern volatile uint8_t djFilter_value;
extern volatile uint8_t djFilter_reso;

/** filters the two interleaved stereo int32 mix buffers of one block in place;
 * returns 1 if it changed them (0 while off) */
uint8_t djFilter_process(int32_t* mix, int32_t* mix2);

#endif /* DJFILTER_H_ */
