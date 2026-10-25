/*
 * VoiceSlot.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * The 6 voice slots. Each slot holds one voice of any engine (drum, snare,
 * cymbal, hi-hat) in a union; the fields all engines share (VoiceCommon) are
 * at the same place whatever engine the slot runs. Tracks 1-6 trigger slots
 * 1-6, track 7 is the second trigger of slot 6 with its own amp decay.
 */

#ifndef VOICESLOT_H_
#define VOICESLOT_H_

#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/HiHat.h"

#define NUM_VOICE_SLOTS	6

//engine numbers are stored in kits and sent via MIDI: never change them
enum VoiceEngine
{
	ENGINE_DRUM		= 0,
	ENGINE_SNARE	= 1,
	ENGINE_CYMBAL	= 2,
	ENGINE_HIHAT	= 3,
	NUM_ENGINES
};

typedef struct
{
	union
	{
		VoiceCommon	c;		//the fields of every engine
		DrumVoice	drum;
		SnareVoice	snare;
		CymbalVoice	cymbal;
		HiHatVoice	hat;
	} u;
	uint8_t		engine;		//enum VoiceEngine
	float		decay[2];	//slot 6 only: amp decay of track 6 [0] and track 7 [1]
} VoiceSlot;

extern VoiceSlot voiceSlots[NUM_VOICE_SLOTS];

//the engine every slot has in the stock firmware (and in kits without engine data)
uint8_t slot_stockEngine(const uint8_t slot);

//initialize all slots with their stock engines
void slot_init(void);

//change the engine of a slot. The voice is re-initialized (LFO and fader
//smoothing are kept); its sound parameters have to be sent again afterwards.
void slot_setEngine(const uint8_t slot, const uint8_t engine);

//trigger track 0-6 (track 6 = 2nd trigger of slot 5)
void slot_trigger(const uint8_t track, const uint8_t vel, const uint8_t note);

#endif /* VOICESLOT_H_ */
