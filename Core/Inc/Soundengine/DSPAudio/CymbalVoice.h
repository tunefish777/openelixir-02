/*
 * CymbalVoice.h
 *
 *  Created on: 16.06.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#ifndef CYMBALVOICE_H_
#define CYMBALVOICE_H_

#include "stm32f7xx_hal.h"
#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Oscillator.h"
#include "config.h"
#include "ResonantFilter.h"
#include "SlopeEg2.h"
#include "random.h"
#include "transientGenerator.h"
#include "BufferTools.h"
#include "lfo.h"
#include "distortion.h"
#include "snapEg.h"
#include "Soundengine/DSPAudio/1PoleLp.h"

typedef struct CymbalStruct
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
} CymbalVoice;

extern CymbalVoice cymbalVoice;


//initialize all the parameters to sane values
void Cymbal_init(CymbalVoice* v, uint8_t slot);

void Cymbal_trigger(CymbalVoice* v,  const uint8_t vel, const uint8_t note);
void Cymbal_updatePan(CymbalVoice* v);
void Cymbal_calcSyncBlock(CymbalVoice* v, int16_t* buf, const uint8_t size);

/** calculate envelopes etc (all 16 samples */
void Cymbal_calcAsync(CymbalVoice* v);

//-0.5 = left 0=both max 0.5=right*/
void Cymbal_setPan(CymbalVoice* v, const uint8_t pan);


#endif /* CYMBALVOICE_H_ */
