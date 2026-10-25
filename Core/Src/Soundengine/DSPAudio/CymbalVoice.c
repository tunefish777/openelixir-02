/*
 * CymbalVoice.c
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


#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/squareRootLut.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "Soundengine/TriggerOut.h"
#include "Soundengine/config.h"
#include "Soundengine/MIDI/MidiVoiceControl.h"
#include "IO/adcPots.h"
#include "Soundengine/FxBus.h"

//---------------------------------------------------
void Cymbal_setPan(CymbalVoice* v, const uint8_t pan)
{
	//v->panL = squareRootLut[127-pan];
	//v->panR = squareRootLut[pan];
	v->pan = pan;
}
//---------------------------------------------------

void Cymbal_init(CymbalVoice* v, uint8_t slot)
{
	v->slot = slot;

	initOnePole(&v->FaderVolumeFilter);
	setOnePoleCoef(&v->FaderVolumeFilter,0.1);

	SnapEg_init(&v->snapEg);
	Cymbal_setPan(v, 0);
	v->vol = 0.8f;

	//v->panModifier = 1.f;

	transient_init(&v->transGen);

	v->fmModAmount1 = 0.5f;
	v->fmModAmount2 = 0.5f;

	setDistortionShape(&v->distortion, 0.f);

	v->modOsc.freq = 440;
	v->modOsc.waveform = SINE;
	v->modOsc.fmMod = 0;
	v->modOsc.midiFreq = 70<<8 | 64;
	v->modOsc.pitchMod = 1.0f;
	v->modOsc.modNodeValue = 1;

	v->modOsc2.freq = 440;
	v->modOsc2.waveform = NOISE;//SINE;
	v->modOsc2.fmMod = 0;
	v->modOsc2.midiFreq = 70<<8| 64;
	v->modOsc2.pitchMod = 1.0f;
	v->modOsc2.modNodeValue = 1;

	v->osc.freq = 440;
	v->osc.waveform = 1;
	v->osc.fmMod = 1;
	v->osc.midiFreq = 70<<8 | 64;
	v->osc.pitchMod = 1.0f;
	v->osc.modNodeValue = 1;

	v->volumeMod = 1;

	slopeEg2_init(&v->oscVolEg);

	SVF_init(&v->filter);

	lfo_init(&v->lfo);

}

//---------------------------------------------------
void Cymbal_updatePan(CymbalVoice* v)
{
	v->triggeredPanL = sqrtf((127-v->pan)/127.f);
	v->triggeredPanR = sqrtf(v->pan/127.f);
}
//---------------------------------------------------
void Cymbal_trigger(CymbalVoice* v, const uint8_t vel, const uint8_t note)
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

	v->modOsc.phase = 0;
	v->modOsc2.phase = 0;

	osc_setBaseNote(&v->osc,note);
	osc_setBaseNote(&v->modOsc,note);
	osc_setBaseNote(&v->modOsc2,note);

	slopeEg2_trigger(&v->oscVolEg);
	v->velo = vel/127.f;

	transient_trigger(&v->transGen);

	SnapEg_trigger(&v->snapEg);

	Cymbal_updatePan(v);
}
//---------------------------------------------------
IN_ITCM_RAM void Cymbal_calcAsync(CymbalVoice* v)
{
	//calc the osc  vol eg
	v->egValueOscVol = slopeEg2_calc(&v->oscVolEg);

	//turn off trigger signal if trigger gate mode is on and volume == 0
	/*
	if(trigger_isGateModeOn())
	{
		if(!v->egValueOscVol) {
			trigger_triggerVoice(TRIGGER_5, TRIGGER_OFF);
			voiceControl_noteOff(TRIGGER_5);
		}
	}
	*/

	//calc snap EG if transient sample 0 is activated
	if(v->transGen.waveform == 0)
	{
		const float snapVal = SnapEg_calc(&v->snapEg,v->transGen.pitch);
		v->osc.pitchMod = 1 + snapVal*v->transGen.volume;
	}

	//update osc phaseInc
	osc_setFreq(&v->osc);
	osc_setFreq(&v->modOsc);
	osc_setFreq(&v->modOsc2);


}
//---------------------------------------------------
void Cymbal_calcSyncBlock(CymbalVoice* v, int16_t* buf, const uint8_t size)
{
	//the fader can't change while a block is calculated: read it once, not per sample
	const float faderVolume = adc_getFaderVolume(v->slot);
		int16_t mod[size];
		int16_t mod2[size];
		//calc next mod osc sample
		calcNextOscSampleBlock(&v->modOsc,mod,size,v->fmModAmount1, v->pwm);
		calcNextOscSampleBlock(&v->modOsc2,mod2,size,v->fmModAmount2, v->pwm);

		//combine both mod oscs to 1 modulation signal
		bufferTool_addBuffersSaturating(mod,mod2,size);

		calcNextOscSampleFmBlock(&v->osc,mod,buf,size,1.f, v->pwm) ;
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

		//calc transient sample
		transient_calcBlock(&v->transGen,mod,size);

		uint8_t j;
		if(v->volumeMod)
		{
			v->combinedVolume = v->vol *v->velo *  v->egValueOscVol ;
			//const float vol = ;

#if 1
			int32_t* x = (int32_t*)buf;
			int32_t* y = (int32_t*)mod;

			//add filter to buffer
			for(uint8_t i=0;i<size/2;i++)
			{
				x[i] = __QADD16(x[i],y[i]);
			}

			for(j=0;j<size;j++)
			{
				//add volume
				buf[j] *=  v->combinedVolume * calcOnePole( &v->FaderVolumeFilter, faderVolume);
			}
#else
			for(j=0;j<size;j++)
			{
				//add filter to buffer
				buf[j] = (__QADD16(buf[j],mod[j])) ;
				buf[j] *=  vol;
			}
#endif
		}
		else
		{
			v->combinedVolume = v->vol * v->egValueOscVol;
			//const float vol = v->combinedVolume ;

#if 1
			int32_t* x = (int32_t*)buf;
			int32_t* y = (int32_t*)mod;

			//add filter to buffer
			for(uint8_t i=0;i<size/2;i++)
			{
				x[i] = __QADD16(x[i],y[i]);
			}

			for(j=0;j<size;j++)
			{
				//add volume
				buf[j] *=  v->combinedVolume * calcOnePole( &v->FaderVolumeFilter, faderVolume);
			}
#else
			for(j=0;j<size;j++)
			{
				//add filter to buffer
				buf[j] = (__QADD16(buf[j],mod[j])) ;
				buf[j] *=  vol;
			}
#endif
		}
		calcDistBlock(&v->distortion,buf,size);
}
//---------------------------------------------------

