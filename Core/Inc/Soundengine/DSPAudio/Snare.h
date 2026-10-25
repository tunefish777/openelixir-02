/*
 * Snare.h
 *
 *  Created on: 16.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#ifndef SNARE_H_
#define SNARE_H_

#include "stm32f7xx_hal.h"
#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Oscillator.h"
#include "config.h"
#include "ResonantFilter.h"
#include "distortion.h"
#include "Decay.h"
#include "SlopeEg2.h"
#include "transientGenerator.h"
#include "BufferTools.h"
#include "lfo.h"
#include "snapEg.h"
#include "Soundengine/DSPAudio/1PoleLp.h"

#define USE_PEAK 0
//------------------------------------------------------------------------
typedef struct SnareStruct
{
	VOICE_COMMON_FIELDS

	// engine specific
	OscInfo 	noiseOsc;	// the noise osc

	//float		panL;		// [0:1]
	//float		panR;		// [0:1]
	//float 		panModifier;

	float   	mix;		// defines the mix between osc and noise [0 = 100% osc: 1 = 100% noise]

	int16_t		oscSample;
	int32_t		noiseSample;

	DecayEg		oscPitchEg;
	float		egPitchModAmount;
} SnareVoice;

//initialize all the parameters to sane values
void Snare_init(SnareVoice* v, uint8_t slot);

void Snare_trigger(SnareVoice* v, const uint8_t vel, const uint8_t note);
void Snare_updatePan(SnareVoice* v);
/** claculate the oscillators and sample based stuff*/
void Snare_calcSyncBlock(SnareVoice* v, int16_t* buf, const uint8_t size);

/** calculate envelopes etc (all 16 samples */
void Snare_calcAsync(SnareVoice* v);

//-0.5 = left 0=both max 0.5=right*/
void Snare_setPan(SnareVoice* v, const uint8_t pan);
#endif /* SNARE_H_ */
