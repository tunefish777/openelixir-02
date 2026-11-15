/*
 * ModalVoice.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */
#include "Soundengine/DSPAudio/ModalVoice.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "IO/adcPots.h"
#include <math.h>
#include <stdlib.h>

#define MODAL_TAIL_BLOCKS	32		//at most ~46 ms: the fade (2 ms), then the filter rings out

//------------------------------------------------------------------------------------
void Modal_init(ModalVoice* v, uint8_t slot)
{
	v->slot = slot;
	initOnePole(&v->FaderVolumeFilter);
	setOnePoleCoef(&v->FaderVolumeFilter,0.1);
	SnapEg_init(&v->snapEg);
	v->pan = 0;
	v->vol = 0.8f;

	v->osc.freq = 440;
	v->osc.waveform = 0;
	v->osc.fmMod = 0;
	v->osc.midiFreq = 70<<8 | 64;
	v->osc.pitchMod = 1.0f;
	v->osc.modNodeValue = 1;

	setDistortionShape(&v->distortion, 0.f);
	v->volumeMod = 1;
	transient_init(&v->transGen);
	slopeEg2_init(&v->oscVolEg);
	SVF_init(&v->filter);
	lfo_init(&v->lfo);
	modal_init(&v->res);
}
//------------------------------------------------------------------------------------
void Modal_trigger(ModalVoice* v, const uint8_t vel, const uint8_t note)
{
	lfo_retrigger(v->slot);
	modNode_updateValue(&velocityModulators[v->slot],vel/127.f);

	v->velo = vel/127.f;
	osc_setBaseNote(&v->osc,note);
	transient_trigger(&v->transGen);
	SnapEg_trigger(&v->snapEg);

	v->triggeredPanL = sqrtf((127-v->pan)/127.f);
	v->triggeredPanR = sqrtf(v->pan/127.f);

	//the envelope and the resonator restart in the audio interrupt (a MIDI note from the
	//main loop could be interrupted half way otherwise)
	v->pending = 1;
}
//------------------------------------------------------------------------------------
//the new strike: envelope, resonator and filter from rest; the previous one goes to the tail
static void Modal_restart(ModalVoice* v)
{
	slopeEg2_trigger(&v->oscVolEg);
	//the envelope would continue an attack from its current level
	if(v->oscVolEg.state == EG_A)
		v->oscVolEg.value = 0;
	if(v->res.active)
	{
		v->tail = v->filter;
		v->tailGain = v->lastVolume;
		v->tailLeft = MODAL_TAIL_BLOCKS;
	}
	SVF_reset(&v->filter);
	//the level follows the velocity like every engine (volumeMod), so the exciter always
	//strikes fully; the hit starts at its level (a ramp would soften a hard attack)
	modal_trigger(&v->res, 1.f);
	v->freshHit = 1;
}
//------------------------------------------------------------------------------------
void Modal_calcAsync(ModalVoice* v)
{
	const uint8_t start = v->pending;
	v->pending = 0;

	//transient "snap" mode: a short pitch drop like the other engines
	v->osc.pitchMod = 1.f;
	if(v->transGen.waveform == 0)
	{
		const float snapVal = SnapEg_calc(&v->snapEg, v->transGen.pitch);
		v->osc.pitchMod += snapVal * v->transGen.volume;
	}
	modal_setFrequency(&v->res, v->osc.freq * v->osc.pitchMod * v->osc.modNodeValue);

	if(start)
		Modal_restart(v);
	v->egValueOscVol = slopeEg2_calc(&v->oscVolEg);

	//during the attack of the amp envelope the strike keeps ringing: a long attack swells it up
	//(otherwise the ringing decays meanwhile and only its quiet tail fades in)
	modal_setHold(&v->res, v->oscVolEg.state == EG_A);
	//the sustained exciter runs until the amp envelope has decayed (it starts at 0 in the attack)
	if(v->oscVolEg.state == EG_STOPPED || (v->oscVolEg.state == EG_D && v->egValueOscVol <= 0.f))
		modal_release(&v->res);
}
//------------------------------------------------------------------------------------
void Modal_calcSyncBlock(ModalVoice* v, int16_t* buf, const uint8_t size)
{
	const float faderVolume = adc_getFaderVolume(v->slot);
	float res[size];
	float fade[size];
	float fader[size];
	int16_t transBuf[size];

	const uint8_t faded = modal_calcBlock(&v->res, res, v->tailLeft ? fade : NULL, size);
	for(uint8_t j=0;j<size;j++)
	{
		buf[j] = (int16_t)(res[j] * 32767.f);
	}
	SVF_calcBlockZDF(&v->filter,v->filterType,buf,size);
	transient_calcBlock(&v->transGen,transBuf,size);

	//the amp envelope advances once per block: ramp the level over the block, steps click
	//on a ringing resonator (e.g. during a short attack)
	v->combinedVolume = v->vol * v->egValueOscVol * (v->volumeMod ? v->velo : 1.f);
	float gain = v->freshHit ? v->combinedVolume : v->lastVolume;
	const float step = (v->combinedVolume - gain) / size;
	v->freshHit = 0;
	for(uint8_t j=0;j<size;j++)
	{
		gain += step;
		fader[j] = calcOnePole(&v->FaderVolumeFilter, faderVolume);
		const float tmp = (float)buf[j] + transBuf[j];
		buf[j] = __SSAT((int32_t)(gain * tmp * fader[j]), 16);
	}
	v->lastVolume = v->combinedVolume;
	calcDistBlock(&v->distortion,buf,size);

	//the previous strike: its fading ringing, then what is left in its filter
	if(v->tailLeft)
	{
		int16_t tailBuf[size];
		for(uint8_t j=0;j<size;j++)
		{
			tailBuf[j] = faded ? (int16_t)(fade[j] * 32767.f) : 0;
		}
		SVF_calcBlockZDF(&v->tail,v->filterType,tailBuf,size);
		int16_t peak = 0;
		for(uint8_t j=0;j<size;j++)
		{
			tailBuf[j] = __SSAT((int32_t)(v->tailGain * tailBuf[j] * fader[j]), 16);
			peak = abs(tailBuf[j]) > peak ? abs(tailBuf[j]) : peak;
		}
		calcDistBlock(&v->distortion,tailBuf,size);
		for(uint8_t j=0;j<size;j++)
		{
			buf[j] = __SSAT(buf[j] + tailBuf[j], 16);
		}
		v->tailLeft = (!faded && peak <= 1) ? 0 : v->tailLeft - 1;
	}
}
