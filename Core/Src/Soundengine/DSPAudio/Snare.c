/*
 * Snare.c
 *
 *  Created on: 17.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */



#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/squareRootLut.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "Soundengine/TriggerOut.h"
#include "Soundengine/MIDI/MidiVoiceControl.h"
#include "IO/adcPots.h"
#include "Soundengine/FxBus.h"
//instance of the snare voice

//---------------------------------------------------
void Snare_setPan(SnareVoice* v, const uint8_t pan)
{
	//v->panL = squareRootLut[127-pan];
	//v->panR = squareRootLut[pan];
	v->pan = pan;
}
//---------------------------------------------------
void Snare_init(SnareVoice* v, uint8_t slot)
{
	v->slot = slot;
	initOnePole(&v->FaderVolumeFilter);
	setOnePoleCoef(&v->FaderVolumeFilter,0.1);

	SnapEg_init(&v->snapEg);
	Snare_setPan(v, 0);
	v->vol = 0.8f;

	//v->panModifier = 1.f;

	v->noiseOsc.freq = 440;
	v->noiseOsc.waveform = 1;
	v->noiseOsc.fmMod = 0;
	v->noiseOsc.midiFreq = 70<<8 | 64;
	v->noiseOsc.pitchMod = 1.0f;
	v->noiseOsc.modNodeValue = 1;

	v->osc.freq = 440;
	v->osc.waveform = 1;
	v->osc.fmMod = 0;
	v->osc.midiFreq = 70<<8 | 64;
	v->osc.modNodeValue = 1;

	setDistortionShape(&v->distortion, 0.f);

	v->volumeMod = 1;

	transient_init(&v->transGen);

	DecayEg_init(&v->oscPitchEg);
	v->egPitchModAmount = 0.5f;

	slopeEg2_init(&v->oscVolEg);



	SVF_init(&v->filter);

	lfo_init(&v->lfo);
}
//---------------------------------------------------
void Snare_updatePan(SnareVoice* v)
{
	v->triggeredPanL = sqrtf((127-v->pan)/127.f);
	v->triggeredPanR = sqrtf(v->pan/127.f);
}
//---------------------------------------------------
void Snare_trigger(SnareVoice* v, const uint8_t vel, const uint8_t note)
{
	lfo_retrigger(v->slot);
	//update velocity modulation
	modNode_updateValue(&velocityModulators[v->slot],vel/127.f);

	float offset = 1;
	if(v->transGen.waveform==1) //offset mode
	{
		offset -= v->transGen.volume;
	}
	if(v->osc.waveform == SINE)
		v->osc.phase = (0x3ff<<20)*offset;//voiceArray[voiceNr].osc.startPhase ;
	else if(v->osc.waveform > SINE && v->osc.waveform <= REC)
		v->osc.phase = (0xff<<22)*offset;
	else
		v->osc.phase = 0;

	DecayEg_trigger(&v->oscPitchEg);
	slopeEg2_trigger(&v->oscVolEg);
	v->velo = vel/127.f;

	osc_setBaseNote(&v->osc,note);
	//TODO noise muss mit transponiert werden

	transient_trigger(&v->transGen);

	SnapEg_trigger(&v->snapEg);

	Snare_updatePan(v);
}
//---------------------------------------------------
IN_ITCM_RAM void Snare_calcAsync(SnareVoice* v)
{
	//add modulation eg to osc freq (1 = no change. a+eg = original freq + modulation
	const float egPitchVal = DecayEg_calc(&v->oscPitchEg);
	const float pitchEgValue = egPitchVal*v->egPitchModAmount;
	v->osc.pitchMod = 1+pitchEgValue;

	//calc the osc  vol eg
	v->egValueOscVol = slopeEg2_calc(&v->oscVolEg);

	//turn off trigger signal if trigger gate mode is on and volume == 0
	/*
	if(trigger_isGateModeOn())
	{
		if(!v->egValueOscVol) {
			//trigger_triggerVoice(TRIGGER_4, TRIGGER_OFF);
			//voiceControl_noteOff(TRIGGER_4);
		}
	}
	*/

	//calc snap EG if transient sample 0 is activated
	if(v->transGen.waveform == 0)
	{
		const float snapVal = SnapEg_calc(&v->snapEg, v->transGen.pitch);
		v->osc.pitchMod += snapVal*v->transGen.volume;;
	}

	osc_setFreq(&v->osc);
	osc_setFreq(&v->noiseOsc);


}
//---------------------------------------------------
void Snare_calcSyncBlock(SnareVoice* v, int16_t* buf, const uint8_t size)
{
	//the fader can't change while a block is calculated: read it once, not per sample
	const float faderVolume = adc_getFaderVolume(v->slot);
	int16_t transBuf[size];
	int16_t oscBuf[size];

	calcNoiseBlock(&v->noiseOsc,buf,size,0.9f);
#if USE_REVERB
	if(fxBus.fxType != FX_REVERB)
	{
#endif
		SVF_calcBlockZDF(&v->filter,v->filterType,buf,size);
#if USE_REVERB
	}
	else
	{
		SVF_calcBlockCheapZDF(&v->filter,v->filterType,buf,size);
	}
#endif




	//calc next osc sample
	calcNextOscSampleBlock(&v->osc,oscBuf,size,(1.f-v->mix), v->pwm);
	// apply filter to synthesized sound as well here if desired, or combine code for more efficiency
	//SVF_calcBlockZDF(&v->filter,v->filterType,transBuf,size);


	//calc transient sample
	transient_calcBlock(&v->transGen,transBuf,size);


	uint8_t j;
	if(v->volumeMod)
	{
		v->combinedVolume = v->vol * v->velo * v->egValueOscVol ;
		//const float vol = ;

		for(j=0;j<size;j++)
		{

#if 1
			//float fmaf(float a, float b, float c);
			//result = a × b + c
			float tmp = fmaf(buf[j], v->mix, oscBuf[j]);
			//add transient sample
			//tmp	= __QADD16(transBuf[j],(int16_t)tmp);
			tmp	+= transBuf[j];
			buf[j] =   v->combinedVolume  * tmp * calcOnePole( &v->FaderVolumeFilter, faderVolume);

#else

			//add filter to buffer
			buf[j] *= v->mix;
			buf[j] = (__QADD16(buf[j],transBuf[j])); //transBuf here is mod osc

			buf[j] *=  vol;
#endif
		}
	}
	else
	{
		v->combinedVolume = v->vol * v->egValueOscVol;
		//const float vol = v->combinedVolume * ;

		for(j=0;j<size;j++)
		{

#if 1
			float tmp = fmaf(buf[j], v->mix, oscBuf[j]);
			//add transient sample
			//tmp	= __QADD16(transBuf[j],(int16_t)tmp);
			tmp	+= transBuf[j];
			buf[j] =  v->combinedVolume * tmp * calcOnePole( &v->FaderVolumeFilter, faderVolume);
#else
			//add filter to buffer
			buf[j] *= v->mix;
			buf[j] = (__QADD16(buf[j],transBuf[j]));
			buf[j] *=  vol;
#endif
		}
	}

	calcDistBlock(&v->distortion,buf,size);
}
//------------------------------------------------------------------------
