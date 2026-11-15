/*
 * ModalVoice.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * The "Modal" voice engine: a struck resonator (ModalResonator) as the sound
 * source, followed by the same filter, transient, amp envelope, drive, LFO,
 * velocity, pan and routing as the other engines. The pitch comes from the
 * voice's oscillator settings (coarse, fine, step note, pitch modulation); the
 * oscillator itself isn't played.
 */
#ifndef MODALVOICE_H_
#define MODALVOICE_H_

#include "Soundengine/DSPAudio/VoiceCommon.h"
#include "Soundengine/DSPAudio/ModalResonator.h"

typedef struct
{
	VOICE_COMMON_FIELDS
	// engine specific
	ModalResonator res;
	float lastVolume;		//level at the end of the last block (the level ramps from there)
	uint8_t freshHit;		//new strike: starts at its level, no ramp
	volatile uint8_t pending;	//triggered, the strike starts in the next block (Modal_calcAsync)
	//tail: the ringing of the previous strike fades out through its own copy of the filter,
	//at its own level, so the new strike starts from rest (filter included)
	ResonantFilter tail;
	float tailGain;
	uint8_t tailLeft;		//blocks
} ModalVoice;

void Modal_init(ModalVoice* v, uint8_t slot);
/** every hit starts the same way, whatever the voice did before; it may be called from the
 *  main loop (MIDI): the restart itself happens in the audio interrupt, see Modal_calcAsync */
void Modal_trigger(ModalVoice* v, const uint8_t vel, const uint8_t note);
/** envelopes and pitch, once per block */
void Modal_calcAsync(ModalVoice* v);
/** the block of the voice */
void Modal_calcSyncBlock(ModalVoice* v, int16_t* buf, const uint8_t size);

#endif /* MODALVOICE_H_ */
