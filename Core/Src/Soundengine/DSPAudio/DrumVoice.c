/*
 * DrumVoice.c
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


#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Oscillator.h"
#include "Soundengine/DSPAudio/random.h"
#include "math.h"
#include "Soundengine/DSPAudio/squareRootLut.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/MIDI/ParameterArray.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "Soundengine/TriggerOut.h"
#include "Soundengine/MIDI/MidiVoiceControl.h"
#include "IO/adcPots.h"

INDTCM static float ampSmoothValue = 0.1f;
//---------------------------------------------------
INDTCMZ DrumVoice voiceArray[NUM_DRUM_VOICES];
//---------------------------------------------------
IN_ITCM_RAM void Drum_setPan(DrumVoice* v, const uint8_t pan)
{
	v->pan = pan;
}
//---------------------------------------------------
void drum_setPhase(DrumVoice* v, const uint8_t phase)
{
	const uint32_t startPhase = (phase/127.f)*0xffffffff;
	v->osc.startPhase = startPhase;
	v->modOsc.startPhase = startPhase;
}
//---------------------------------------------------
void Drum_init(DrumVoice* v, uint8_t slot)
{
	v->slot = slot;

	initOnePole(&v->FaderVolumeFilter);
	setOnePoleCoef(&v->FaderVolumeFilter,0.01);

#if SMOOTH_MIXER_VOL_PAN_DRIVE
	for(uint8_t j=0;j<2;j++)
	{
		initOnePole(&v->MixerPageFilter[j]);
		setOnePoleCoef(&v->MixerPageFilter[j],0.01);
	}
	initOnePole(&v->distortion.DriveParamFilter);
	setOnePoleCoef(&v->distortion.DriveParamFilter,0.01);

#endif

	SnapEg_init(&v->snapEg);
	Drum_setPan(v,0.f);
	v->vol = 0.8f;
	//v->panModifier = 1.f;
	v->fmModAmount = 0.5f;
	transient_init(&v->transGen);
#if ENABLE_DRUM_SVF
	SVF_init(&v->filter);
	v->filterType = 0x01;
#endif
	lfo_init(&v->lfo);

	v->modOsc.freq = 440;
	v->modOsc.waveform = 1;
	v->modOsc.fmMod = 0;
	v->modOsc.midiFreq = 70<<8 | 64;
	v->modOsc.pitchMod = 1.0f;
	v->modOsc.modNodeValue = 1;

	v->volumeMod = 1;

	v->osc.freq = 440;
	v->osc.modNodeValue = 1;
	v->osc.waveform = TRI+slot; //for testing init to tri,saw,rec
	v->osc.fmMod = 0;
	v->osc.midiFreq = 70<<8 | 64;

	DecayEg_init(&v->oscPitchEg);
	v->egPitchModAmount = 0.5f;

	slopeEg2_init(&v->oscVolEg);
	setDistortionShape(&v->distortion, 0.f);

#ifdef USE_AMP_FILTER
	initOnePole(&v->ampFilter);
	setOnePoleCoef(&v->ampFilter,ampSmoothValue);
#endif

#if ENABLE_MIX_OSC
	v->mixOscs = true;
#endif
	v->decimationCnt = 0;
	v->decimationRate = 1;

}
//---------------------------------------------------
void initDrumVoice()
{
	ampSmoothValue = 0.1f;




	uint8_t i;
	for(i=0;i<NUM_DRUM_VOICES;i++)
	{
		Drum_init(&voiceArray[i], i);
	}
}
//---------------------------------------------------
void Drum_updatePan(DrumVoice* v)
{
	v->triggeredPanL = sqrtf((127-v->pan)/127.f);
	v->triggeredPanR = sqrtf(v->pan/127.f);
}
//---------------------------------------------------
void Drum_trigger(DrumVoice* v, const uint8_t vol, const uint8_t note)
{

	lfo_retrigger(v->slot);

	//update velocity modulation
	modNode_updateValue(&velocityModulators[v->slot],vol/127.f);

	//only reset phase if envelope is closed
#ifdef USE_AMP_FILTER
	if((v->volEgValueBlock[15]<=0.01f) || (v->transGen.waveform==1))
#else
		//if((v->ampFilterInput<=0.01f) || (v->transGen.waveform==1))
#endif
	{
		float offset = 1;
		if(v->transGen.waveform==1) //offset mode
		{
			offset -= v->transGen.volume;
#ifdef USE_AMP_FILTER
			setOnePoleCoef(&v->ampFilter,1.0f); //turn off amp filter for super snappy attack

		} else {
			setOnePoleCoef(&v->ampFilter,ampSmoothValue);
#endif
		}
		if(v->osc.waveform == SINE)
			v->osc.phase = 1024 + ( (0x3ff<<20) - 1024)*offset;//v->osc.startPhase ;
		else if(v->osc.waveform > SINE && v->osc.waveform <= REC)
			v->osc.phase = (0xff<<22)*offset;
		else
			v->osc.phase = 0;

		//reset mod osc phase
		if(v->modOsc.waveform == SINE)
			v->modOsc.phase = 1024 + ( (0x3ff<<20) - 1024);//v->osc.startPhase ;
		else if(v->modOsc.waveform > SINE && v->modOsc.waveform <= REC)
			v->modOsc.phase = (0xff<<22);
		else
			v->modOsc.phase = 0;

	}

	osc_setBaseNote(&v->osc,note);
	osc_setBaseNote(&v->modOsc,note);


	DecayEg_trigger(&v->oscPitchEg);
	slopeEg2_trigger(&v->oscVolEg);
	v->velo = vol/127.f;

	transient_trigger(&v->transGen);

	SnapEg_trigger(&v->snapEg);

	//reset filter coeffs to prevent wrong transient
	SVF_reset(&v->filter);

	//update pan value
	Drum_updatePan(v);
}
//---------------------------------------------------
IN_ITCM_RAM void calcDrumVoiceAsync(DrumVoice* v)
{


	//add modulation eg to osc freq (1 = no change. a+eg = original freq + modulation
	const float egPitchVal = DecayEg_calc(&v->oscPitchEg);
	const float pitchEgValue = egPitchVal*v->egPitchModAmount;
	v->osc.pitchMod = 1+pitchEgValue;

	//calc snap EG if transient sample 0 is activated
	if(v->transGen.waveform == 0)
	{
		const float snapVal = SnapEg_calc(&v->snapEg, v->transGen.pitch);
		v->osc.pitchMod += snapVal*v->transGen.volume;
	}

	// fm amount with pitch eg
	v->osc.fmMod = v->fmModAmount * egPitchVal;

	//calc the osc + noise vol eg
#if (AMP_EG_SYNC==0)

	//check if in attack phase
	if( (v->oscVolEg.attack == 1 ) && ((v->oscVolEg.state == EG_A) || (v->oscVolEg.state == EG_REPEAT)) )
	{
			//if attack is set to 0 -> no interpolation
		v->ampFilterInput = slopeEg2_calc(&v->oscVolEg);
		v->lastGain = v->ampFilterInput;
	}
	else
	{
		v->lastGain = v->ampFilterInput;
		v->ampFilterInput = slopeEg2_calc(&v->oscVolEg);
	}

	//turn off trigger signal if trigger gate mode is on and volume == 0
	/*
	if(trigger_isGateModeOn())
	{
		if(!v->ampFilterInput) {
			//trigger_triggerVoice(TRIGGER_1 + v->slot, TRIGGER_OFF);
			voiceControl_noteOff(TRIGGER_1 + v->slot);
		}
	}
	*/
#endif

	//update osc phaseInc
	osc_setFreq(&v->osc);
	osc_setFreq(&v->modOsc);



}

//---------------------------------------------------
IN_ITCM_RAM void calcDrumVoiceSyncBlock(DrumVoice* v, int16_t* buf, const uint8_t size)
{


	int16_t modBuf[size];

	//calc vol EG
#ifdef USE_AMP_FILTER
	calcOnePoleBlockFixedInput(&v->ampFilter, v->ampFilterInput,v->volEgValueBlock, size);
#endif

	//calc next mod osc sampleBlock - int16_t processing
	calcNextOscSampleBlock(&v->modOsc,modBuf,size,v->fmModAmount, v->pwm);

	if(v->mixOscs)
	{
		//calc main osc buffer - int16_t processing
		calcNextOscSampleBlock(&v->osc,buf,size, (1.f-v->fmModAmount), v->pwm);
		//add mod buffer to main osc buffer - int16_t processing
		bufferTool_addBuffersSaturating(buf,modBuf,size);
	}
	else
	{
		calcNextOscSampleFmBlock(&v->osc,modBuf,buf,size,1.0f, v->pwm);
	}

	//calc transient sample - int16_t processing
	transient_calcBlock(&v->transGen,modBuf,size);

	//Mix with transient buffer - int16_t processing
	bufferTool_addBuffersSaturating(buf,modBuf,size);

	//calc filter block - float processing
	SVF_calcBlockZDF(&v->filter,v->filterType,buf,size);

	//attentuate main OSCs by amp EG
#ifdef USE_AMP_FILTER
	bufferTool_multiplyWithFloatBufferDithered(&v->dither, buf,v->volEgValueBlock,size);
#else
	//bufferTool_addGainInterpolatedDoubleDithered(&v->dither, buf,v->ampFilterInput, v->lastGain, size);
	//float
	bufferTool_addGainInterpolated(buf,v->ampFilterInput, v->lastGain, size);

	//buf[i] = dither_process(v->dither, buf[i]
#endif

	//MIDI velocity - float processing
	if(v->volumeMod)
	{
		bufferTool_addGain(buf,v->velo,size);
	}
	//distortion - float processing
#if (USE_FILTER_DRIVE == 0)
#if SMOOTH_MIXER_VOL_PAN_DRIVE
	calcDistBlockFiltered(&v->distortion,buf,size);
#else
	calcDistBlock(&v->distortion,buf,size);
#endif
#endif
	//channel volume
	//bufferTool_addGain(buf,v->combinedVolume  ,size);

	//the fader can't change while a block is calculated: read it once, not per sample
	const float faderVolume = adc_getFaderVolume(v->slot);
	for(int i=0; i< size; i++)
	{
#if SMOOTH_MIXER_VOL_PAN_DRIVE

		float mixVol 	= calcOnePole( &v->MixerPageFilter[0], v->vol);
		v->combinedVolume = mixVol * calcOnePole( &v->FaderVolumeFilter, faderVolume);
#else
		v->combinedVolume = v->vol * calcOnePole( &v->FaderVolumeFilter, faderVolume);
#endif
		buf[i] *= v->combinedVolume;
	}
}
//---------------------------------------------------


