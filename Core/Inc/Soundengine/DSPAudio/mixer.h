/*
 * mixer.h
 *
 *  Created on: 11.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#ifndef MIXER_H_
#define MIXER_H_

#include "stm32f7xx_hal.h"

#define USE_SWITCH_ROUTING 1
#define USE_DECIMATOR 1
#define USE_TRIGGERED_PAN 1

#if USE_DECIMATOR
extern float mixer_decimation_rate[7];		/**<sets the sample rate decimation. 0..1 = full rate*/
#endif

extern uint8_t mixer_audioRouting[6];

enum
{
	MIXER_ROUTING_DAC1_STEREO,
	MIXER_ROUTING_DAC2_STEREO,
	MIXER_ROUTING_DAC1_L,
	MIXER_ROUTING_DAC1_R,
	MIXER_ROUTING_DAC2_L,
	MIXER_ROUTING_DAC2_R,
	MIXER_ROUTING_FX_BUS,
};

void mixer_init();
void  mixer_calcNextSampleBlock(int16_t* output,int16_t* output2);

/* The voices and the FX return are summed into 32-bit buffers (interleaved
 * L/R like the DMA buffers), so that the sum can exceed the 16-bit range
 * without wrapping or clipping. A limiter per DAC then brings it back into
 * range before it is written to the DMA buffers. */

/** 1 = 32-bit mix with an output limiter per DAC, 0 = the stock 16-bit saturating mix */
extern uint8_t mixer_limiterOn;

/** Adds a packed stereo sample (as built with __PKHBT(left, right, 16)) to a 32-bit mix buffer. */
static inline void mixer_addStereo(int32_t* out, int32_t x)
{
	out[0] += (int16_t)x;
	out[1] += x >> 16;
}

#endif /* MIXER_H_ */
