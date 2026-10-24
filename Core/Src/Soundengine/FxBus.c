/*
 * FxBus.c
 *
 *  Created on: 26.08.2020
 *      Author: julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#include "Soundengine/FxBus.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/DSPAudio/mixer.h"
INDTCMZ FxBus fxBus;

void FxBus_init()
{
	fxBus.dryWet = 0;
	fxBus.audioRouting = 0;
	fxBus.fxType = FX_OFF;

	fxBus.delay.delayType = DELAY_NORMAL;

	Distortion_init(&fxBus.distortion);
	Ringmod_init(&fxBus.ringmod);
	Compressor_init(&fxBus.compressor);
	Delay_init(&fxBus.delay);



#if USE_REVERB
	Reverb_init(&fxBus.reverb);
#endif
}

IN_ITCM_RAM void FxBus_process(int16_t *fxBuffer, int32_t* outL,int32_t* outR,int32_t* outL2, int32_t* outR2, uint32_t size)
{
	int32_t* out;
	int32_t* out2 = NULL;
	// < MIXER_ROUTING_DAC1_L -> stereo 1 or 2
	const uint8_t channels = fxBus.audioRouting < MIXER_ROUTING_DAC1_L ? 2 : 1;
	switch(fxBus.audioRouting)
	{
	default:
	case MIXER_ROUTING_DAC1_STEREO:
		out = outL2;
		out2 = outR2;
		break;
	case MIXER_ROUTING_DAC2_STEREO:
		out = outL;
		out2 = outR;
		break;
	case MIXER_ROUTING_DAC1_L:
		out = outL2;
		break;
	case MIXER_ROUTING_DAC1_R:
		out = outR2;
		break;
	case MIXER_ROUTING_DAC2_L:
		out = outL;
		break;
	case MIXER_ROUTING_DAC2_R:
		out = outR;
		break;

	}

	switch(fxBus.fxType)
	{
	default:
	case FX_OFF:
	{
		if(channels == 2)
		{
			for(uint8_t i=0;i<size*2;i++)
			{
				out[i] += fxBuffer[i];
			}
		}
		else
		{
			//mono out: left channel of the FX bus
			for(uint8_t i=0;i<size;i++)
			{
				*out += fxBuffer[i*2];
				out +=2;
			}
		}
	}
	break;

	case FX_DISTORTION:
		Distortion_process(&fxBus.distortion, fxBuffer, out, out2, size, channels, fxBus.dryWet);
		break;

	case FX_RING:
		Ringmod_process(&fxBus.ringmod, fxBuffer, out, out2, size, channels, fxBus.dryWet);
		break;

	case FX_COMPRESSOR:
		Compressor_process(&fxBus.compressor, fxBuffer, out, out2, size, channels, fxBus.dryWet);
		break;

	case FX_DELAY:
		if(fxBus.delay.delayType == DELAY_NORMAL)
		{
			Delay_process(&fxBus.delay, fxBuffer, out, out2, size, channels, fxBus.dryWet);
		}
		else
		{
			Delay_processPingPong(&fxBus.delay, fxBuffer, out, out2, size, channels, fxBus.dryWet);
		}
		break;
#if USE_REVERB
	case FX_REVERB:
		Reverb_process(&fxBus.reverb, fxBuffer, out, out2, size, channels, fxBus.dryWet);
		break;
#endif

	}

}

void FxBus_prepareFxChange()
{
	//memset(FX_BUFFER, 0, FX_BUFFER_SIZE*sizeof(__fp16) );
//	Ringmod_resetPhase(&fxBus.ringmod);
}

IN_ITCM_RAM void FxBus_updateParameter()
{
	switch(fxBus.fxType)
	{
	case FX_OFF:

		break;

	case FX_DISTORTION:
		Distortion_setDrive(&fxBus.distortion, fxBus.parameter[0] );
		Distortion_setParam1(&fxBus.distortion, fxBus.parameter[1] );
		Distortion_setParam2(&fxBus.distortion, fxBus.parameter[2] );
		Distortion_setParam3(&fxBus.distortion, fxBus.parameter[3] );
		break;

	case FX_RING:
		Ringmod_setFreq(&fxBus.ringmod, fxBus.parameter[0] );
		//Ringmod_setWave(&fxBus.ringmod, fxBus.parameter[1] );
		break;

	case FX_COMPRESSOR:

		Compressor_setAttack(&fxBus.compressor , fxBus.parameter[0]);
		Compressor_setRelease(&fxBus.compressor , fxBus.parameter[1]);
		Compressor_setTreshold(&fxBus.compressor , fxBus.parameter[2]);
		Compressor_setGain(&fxBus.compressor , fxBus.parameter[3]);
		break;

	case FX_DELAY:
		Delay_setRange(&fxBus.delay, fxBus.parameter[1]);
		Delay_setDelayTime(&fxBus.delay, fxBus.parameter[0]);
		Delay_setFilter(&fxBus.delay, fxBus.parameter[2]);
		Delay_setFeedback(&fxBus.delay, fxBus.parameter[3]);

		break;
#if USE_REVERB
	case FX_REVERB:
		Reverb_setDelayTime(&fxBus.reverb, fxBus.parameter[0]);
		Reverb_setFeedback(&fxBus.reverb, fxBus.parameter[1]);
		Reverb_setComposition(&fxBus.reverb, fxBus.parameter[2]);

		Reverb_setVariation(&fxBus.reverb, fxBus.parameter[3]);
		break;
#endif
	default:
		break;
	}
}

IN_ITCM_RAM void FxBus_setParameter(uint8_t idx, float value)
{
	fxBus.parameter[idx] = value;

	FxBus_updateParameter();


}


