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

//roles:	drum				snare				cymbal / hi-hat
// 0		mod osc wave		-					mod osc 1 wave
// 1		pitch EG decay		pitch EG decay		mod osc 2 wave
// 2		pitch EG amount		pitch EG amount		mod osc 2 freq
// 3		pitch EG slope		pitch EG slope		mod osc 2 gain
// 4		FM amount			tone/noise mix		mod osc 1 gain
// 5		FM freq				noise freq			mod osc 1 freq
// 6		mix/FM mode			repeat				repeat (cymbal) / - (hi-hat)
const uint8_t slot_rolePar[NUM_VOICE_SLOTS][NUM_SLOT_ROLES] =
{
	{ PAR_MOD_WAVE_DRUM1, PAR_MOD_EG1, PAR_MODAMNT1, PAR_PITCH_SLOPE1, PAR_FMAMNT1, PAR_FM_FREQ1, PAR_MIX_MOD_1 },
	{ PAR_MOD_WAVE_DRUM2, PAR_MOD_EG2, PAR_MODAMNT2, PAR_PITCH_SLOPE2, PAR_FMAMNT2, PAR_FM_FREQ2, PAR_MIX_MOD_2 },
	{ PAR_MOD_WAVE_DRUM3, PAR_MOD_EG3, PAR_MODAMNT3, PAR_PITCH_SLOPE3, PAR_FMAMNT3, PAR_FM_FREQ3, PAR_MIX_MOD_3 },
	{ PAR_UNUSED_3, PAR_MOD_EG4, PAR_MODAMNT4, PAR_PITCH_SLOPE4, PAR_MIX1, PAR_NOISE_FREQ1, PAR_REPEAT4 },
	{ PAR_WAVE2_CYM, PAR_WAVE3_CYM, PAR_MOD_OSC_F2_CYM, PAR_MOD_OSC_GAIN2_CYM, PAR_MOD_OSC_GAIN1_CYM, PAR_MOD_OSC_F1_CYM, PAR_REPEAT5 },
	{ PAR_WAVE2_HH, PAR_WAVE3_HH, PAR_MOD_OSC_F2, PAR_MOD_OSC_GAIN2, PAR_MOD_OSC_GAIN1, PAR_MOD_OSC_F1, PAR_UNUSED_4 },
};
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
	if(s->engine == engine)
		return;

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
//---------------------------------------------------
static inline float slot_calcPitchModAmount(const uint8_t data2)
{
	const float val = data2/127.f;
	return val*val*PITCH_AMOUNT_FACTOR;
}
//---------------------------------------------------
//A role can be a 0-127 setting for one engine and a waveform for another, so
//clamp: the oscillators write nothing for waveforms >= NUM_OSC_WAVEFORMS.
static uint8_t slot_waveform(const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	const uint8_t wave = isFloatModulator ? NUM_OSC_WAVEFORMS * flt : itg;
	return wave >= NUM_OSC_WAVEFORMS ? NUM_OSC_WAVEFORMS-1 : wave;
}
//---------------------------------------------------
//set the coarse tuning (upper byte of midiFreq) of an oscillator
static void slot_setCoarse(OscInfo* osc, const uint8_t itg)
{
	osc->midiFreq &= 0x00ff;
	osc->midiFreq |= itg << 8;
	osc_recalcFreq(osc);
}
//---------------------------------------------------
static void slot_setDrumRole(DrumVoice* v, const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:	v->modOsc.waveform = slot_waveform(itg, flt, isFloatModulator);	break;
	case 1:	DecayEg_setDecay(&v->oscPitchEg, itg);							break;
	case 2:	v->egPitchModAmount = slot_calcPitchModAmount(itg);				break;
	case 3:	DecayEg_setSlope(&v->oscPitchEg, itg);							break;
	case 4:	v->fmModAmount = flt;											break;
	case 5:	slot_setCoarse(&v->modOsc, itg);								break;
	default: v->mixOscs = itg != 0;											break;
	}
}
//---------------------------------------------------
static void slot_setSnareRole(SnareVoice* v, const uint8_t role, const uint8_t itg, const float flt)
{
	switch(role)
	{
	case 0:																	break;
	case 1:	DecayEg_setDecay(&v->oscPitchEg, itg);							break;
	case 2:	v->egPitchModAmount = slot_calcPitchModAmount(itg);				break;
	case 3:	DecayEg_setSlope(&v->oscPitchEg, itg);							break;
	case 4:	v->mix = flt;													break;
	case 5:	v->noiseOsc.freq = flt*22000;									break;
	default: v->oscVolEg.repeat = itg;										break;
	}
}
//---------------------------------------------------
//cymbal and hi-hat: 2 modulator oscillators
static void slot_setFmRole(OscInfo* modOsc, OscInfo* modOsc2, float* modAmount1, float* modAmount2,
		const uint8_t role, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	switch(role)
	{
	case 0:	modOsc->waveform = slot_waveform(itg, flt, isFloatModulator);	break;
	case 1:	modOsc2->waveform = slot_waveform(itg, flt, isFloatModulator);	break;
	case 2:	slot_setCoarse(modOsc2, itg);									break;
	case 3:	*modAmount2 = flt;												break;
	case 4:	*modAmount1 = flt;												break;
	default: slot_setCoarse(modOsc, itg);									break;
	}
}
//---------------------------------------------------
uint8_t slot_setRoleParameter(const uint16_t par, const uint8_t itg, const float flt, const uint8_t isFloatModulator)
{
	for(uint8_t slot=0;slot<NUM_VOICE_SLOTS;slot++)
	{
		for(uint8_t role=0;role<NUM_SLOT_ROLES;role++)
		{
			if(slot_rolePar[slot][role] != par)
				continue;

			VoiceSlot* s = &voiceSlots[slot];
			switch(s->engine)
			{
			case ENGINE_SNARE:
				slot_setSnareRole(&s->u.snare, role, itg, flt);
				break;
			case ENGINE_CYMBAL:
				if(role == 6)
					s->u.cymbal.oscVolEg.repeat = itg;
				else
					slot_setFmRole(&s->u.cymbal.modOsc, &s->u.cymbal.modOsc2, &s->u.cymbal.fmModAmount1,
							&s->u.cymbal.fmModAmount2, role, itg, flt, isFloatModulator);
				break;
			case ENGINE_HIHAT:
				if(role != 6)
					slot_setFmRole(&s->u.hat.modOsc, &s->u.hat.modOsc2, &s->u.hat.fmModAmount1,
							&s->u.hat.fmModAmount2, role, itg, flt, isFloatModulator);
				break;
			default:
				slot_setDrumRole(&s->u.drum, role, itg, flt, isFloatModulator);
				break;
			}
			return 1;
		}
	}
	return 0;
}
