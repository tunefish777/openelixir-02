/*
 * DrumVoice.h
 *
 *  Created on: 03.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#ifndef DRUMVOICE_H_
#define DRUMVOICE_H_

#include "Soundengine/DSPAudio/Oscillator.h"
#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/config.h"
#include "Soundengine/DSPAudio/ResonantFilter.h"
#include "Soundengine/DSPAudio/distortion.h"
#include "Soundengine/DSPAudio/SlopeEg2.h"
#include "Soundengine/DSPAudio/Decay.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "Soundengine/DSPAudio/lfo.h"
#include "Soundengine/DSPAudio/1PoleLp.h"
#include "Soundengine/DSPAudio/transientGenerator.h"
#include "Soundengine/DSPAudio/snapEg.h"
#include "Soundengine/DSPAudio/dither.h"
//------------------------------------------------------------------------
enum
{
	IS_PLAYING = 0x1,
};
//------------------------------------------------------------------------
typedef struct VoiceStruct
{
	VOICE_COMMON_FIELDS

	// engine specific
	OscInfo 	modOsc;		// the mod osc

	float		fmModAmount;

	//float 		panModifier;

	int16_t 	oscSample;

	DecayEg		oscPitchEg;
	float		egPitchModAmount;
	float 		offset;				//the start offset for the waveform - used to generate clicks

	float 		volEgValueBlock[OUTPUT_DMA_SIZE];

#ifdef USE_AMP_FILTER
	OnePoleFilter ampFilter;
#endif

#if (AMP_EG_SYNC==0)
	double ampFilterInput;
	double lastGain;
#endif

#if ENABLE_MIX_OSC
	uint8_t mixOscs;
#endif

	float decimationCnt;
	float decimationRate;

	Dither dither;

#if SMOOTH_MIXER_VOL_PAN_DRIVE
	OnePoleFilter MixerPageFilter[2];
#endif
} DrumVoice;
//------------------------------------------------------------------------


//initialize all the parameters to sane values
/** initialize one drum voice instance for the given voice slot */
void Drum_init(DrumVoice* v, uint8_t slot);
void Drum_trigger(DrumVoice* v, const uint8_t vol, const uint8_t note);

void Drum_updatePan(DrumVoice* v);

/** block based calculation*/
void calcDrumVoiceSyncBlock(DrumVoice* v, int16_t* buf, const uint8_t size);

/** calculate envelopes etc (all 16 samples */
void calcDrumVoiceAsync(DrumVoice* v);

//-0.5 = left 0=both max 0.5=right*/
void Drum_setPan(DrumVoice* v, const uint8_t pan);

/** set the oscillator start phase for all OSCs*/
void drum_setPhase(DrumVoice* v, const uint8_t phase);

#endif /* DRUMVOICE_H_ */
