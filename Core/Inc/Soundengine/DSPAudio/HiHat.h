/*
 * HiHat.h
 *
 *  Created on: 18.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#ifndef HIHAT_H_
#define HIHAT_H_

#include "stm32f7xx_hal.h"
#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/Oscillator.h"
#include "Soundengine/config.h"
#include "Soundengine/DSPAudio/ResonantFilter.h"
#include "Soundengine/DSPAudio/SlopeEg2.h"
#include "Soundengine/DSPAudio/random.h"
#include "Soundengine/DSPAudio/transientGenerator.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/DSPAudio/lfo.h"
#include "Soundengine/DSPAudio/distortion.h"
#include "Soundengine/DSPAudio/snapEg.h"
#include "Soundengine/DSPAudio/1PoleLp.h"

//#define NUM_HAT 1

typedef struct HiHatStruct
{
	VOICE_COMMON_FIELDS

	// engine specific

	OscInfo 	modOsc;		// the mod osc1
	OscInfo 	modOsc2;	// the mod osc2
	float		fmModAmount1;
	float		fmModAmount2;

	//float		panL;		// [0:1]
	//float		panR;		// [0:1]
	//float 		panModifier;

	int32_t		noiseSample;

	float		decayClosed;
	float		decayOpen;

	uint8_t isOpen;		//is 1 if urrent playing voice is open hihat, 0 for closed
} HiHatVoice;

extern HiHatVoice hatVoice;

//initialize all the parameters to sane values
void HiHat_init(HiHatVoice* v, uint8_t slot);

void HiHat_trigger(HiHatVoice* v, uint8_t vel, uint8_t isOpen, const uint8_t note);
void HiHat_updatePan(HiHatVoice* v);
void HiHat_calcSyncBlock(HiHatVoice* v, int16_t* buf, const uint8_t size);

/** calculate envelopes etc (all 16 samples */
void HiHat_calcAsync(HiHatVoice* v);

//-0.5 = left 0=both max 0.5=right*/
void HiHat_setPan(HiHatVoice* v, const uint8_t pan);

#endif /* HIHAT_H_ */
