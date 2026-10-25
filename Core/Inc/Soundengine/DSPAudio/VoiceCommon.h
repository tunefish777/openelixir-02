/*
 * VoiceCommon.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * Fields that every voice engine (drum, snare, cymbal, hi-hat) has. Each
 * engine struct starts with VOICE_COMMON_FIELDS, so these fields have the same
 * layout in all of them and can be accessed through a VoiceCommon pointer
 * whatever engine a voice slot runs.
 */

#ifndef VOICECOMMON_H_
#define VOICECOMMON_H_

#include "stm32f7xx_hal.h"
#include "Soundengine/DSPAudio/Oscillator.h"
#include "Soundengine/DSPAudio/ResonantFilter.h"
#include "Soundengine/DSPAudio/distortion.h"
#include "Soundengine/DSPAudio/SlopeEg2.h"
#include "Soundengine/DSPAudio/lfo.h"
#include "Soundengine/DSPAudio/1PoleLp.h"
#include "Soundengine/DSPAudio/transientGenerator.h"
#include "Soundengine/DSPAudio/snapEg.h"

#define VOICE_COMMON_FIELDS \
	uint8_t		slot;			/* voice slot 0..5 (LFO, velocity modulator, fader index) */ \
	OscInfo		osc;			/* the main oscillator */ \
	float		vol;			/* volume of the voice */ \
	float		velo;			/* velocity of the last trigger [0:1] */ \
	uint8_t		pan; \
	float		triggeredPanL;	/* only updated with the latest pan value when triggered */ \
	float		triggeredPanR; \
	ResonantFilter filter; \
	uint8_t		filterType;		/* bit 0 = lp, bit 1 = hp, bit 3 = bp on/off */ \
	Lfo			lfo; \
	TransientGenerator transGen; \
	Distortion	distortion; \
	SlopeEg2	oscVolEg;		/* amp envelope */ \
	float		egValueOscVol; \
	uint8_t		volumeMod;		/* modulate volume by midi velocity if 1 */ \
	SnapEg		snapEg; \
	float		combinedVolume; \
	OnePoleFilter FaderVolumeFilter; \
	float		pwm;

typedef struct
{
	VOICE_COMMON_FIELDS
} VoiceCommon;

/** the common part of the voice in each voice slot 0-5 (defined in mixer.c) */
extern VoiceCommon* const voiceCommon[6];

#endif /* VOICECOMMON_H_ */
