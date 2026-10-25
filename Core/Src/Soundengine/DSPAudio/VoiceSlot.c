/*
 * VoiceSlot.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "Soundengine/DSPAudio/VoiceSlot.h"
#include <string.h>

INDTCMZ VoiceSlot voiceSlots[NUM_VOICE_SLOTS];
//---------------------------------------------------
uint8_t slot_stockEngine(const uint8_t slot)
{
	switch(slot)
	{
	case 3:		return ENGINE_SNARE;
	case 4:		return ENGINE_CYMBAL;
	case 5:		return ENGINE_HIHAT;
	default:	return ENGINE_DRUM;
	}
}
//---------------------------------------------------
static void slot_initEngine(VoiceSlot* s, const uint8_t slot)
{
	switch(s->engine)
	{
	case ENGINE_SNARE:	Snare_init(&s->u.snare, slot);		break;
	case ENGINE_CYMBAL:	Cymbal_init(&s->u.cymbal, slot);	break;
	case ENGINE_HIHAT:	HiHat_init(&s->u.hat, slot);		break;
	default:			Drum_init(&s->u.drum, slot);		break;
	}
}
//---------------------------------------------------
void slot_init(void)
{
	for(uint8_t i=0;i<NUM_VOICE_SLOTS;i++)
	{
		voiceSlots[i].engine = slot_stockEngine(i);
		slot_initEngine(&voiceSlots[i], i);
	}
}
//---------------------------------------------------
void slot_setEngine(const uint8_t slot, const uint8_t engine)
{
	if(slot >= NUM_VOICE_SLOTS || engine >= NUM_ENGINES)
		return;

	VoiceSlot* s = &voiceSlots[slot];

	//the audio block is rendered in an interrupt: don't let it see a half initialized voice
	const uint32_t primask = __get_PRIMASK();
	__disable_irq();

	const Lfo lfo = s->u.c.lfo;
	const double faderVolume = s->u.c.FaderVolumeFilter.b;

	//start from zero like at power-on, so no field keeps bytes of the previous engine
	memset(&s->u, 0, sizeof(s->u));
	s->engine = engine;
	slot_initEngine(s, slot);

	//keep the LFO (settings, target, phase) and the current fader level
	s->u.c.lfo = lfo;
	s->u.c.FaderVolumeFilter.b = faderVolume;

	__set_PRIMASK(primask);
}
//---------------------------------------------------
void slot_trigger(const uint8_t track, const uint8_t vel, const uint8_t note)
{
	const uint8_t slot = track < NUM_VOICE_SLOTS ? track : NUM_VOICE_SLOTS-1;
	const uint8_t second = track >= NUM_VOICE_SLOTS;	//track 7
	VoiceSlot* s = &voiceSlots[slot];

	switch(s->engine)
	{
	case ENGINE_SNARE:	Snare_trigger(&s->u.snare, vel, note);				break;
	case ENGINE_CYMBAL:	Cymbal_trigger(&s->u.cymbal, vel, note);			break;
	case ENGINE_HIHAT:	HiHat_trigger(&s->u.hat, vel, second, note);		break;
	default:			Drum_trigger(&s->u.drum, vel, note);				break;
	}

	//tracks 6 and 7 share slot 6 but each has its own amp decay. Set after the
	//trigger, which applies the velocity modulation (it may target the decay).
	if(slot == NUM_VOICE_SLOTS-1)
	{
		s->u.c.oscVolEg.decay = s->decay[second];
	}
}
