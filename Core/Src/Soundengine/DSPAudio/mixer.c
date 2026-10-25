/*
 * mixer.c
 *
 *  Created on: 11.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */

#include "Soundengine/DSPAudio/mixer.h"
#include "Soundengine/config.h"
#include "Soundengine/AudioCodecManager.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/DSPAudio/squareRootLut.h"
#include "main.h"
#include "Soundengine/TriggerOut.h"
#include "menu/menu.h"
#include "Soundengine/FxBus.h"
#include "Soundengine/CpuMeter.h"

	uint8_t r2_Available;
	uint8_t l1_Available;
	uint8_t l2_Available;
	uint8_t r1_Available;


//-----------------------------------------------------------------------
INDTCMZ uint8_t mixer_audioRouting[6];
//-----------------------------------------------------------------------
#if USE_DECIMATOR
INDTCMZ float mixer_decimation_rate[7];		/**<sets the sample rate decimation. 0..1 = full rate*/
INDTCMZ float mixer_decimation_cnt[6];		/**<s'n'h counter for decimator*/
INDTCMZ int16_t mixer_voice_samples[6];		/**< stores the last outputted sample of the 6 voices*/
#endif
//-----------------------------------------------------------------------
void mixer_init()
{
#if USE_DECIMATOR
	int i;
	for(i=0;i<6;i++)
	{
		mixer_decimation_rate[i] 	= 1;
		mixer_decimation_cnt[i] 	= 0;
		mixer_voice_samples[i] 		= 0;
		mixer_audioRouting[i]		= 0;
	}
	mixer_decimation_rate[6] 		= 1;
#endif

	FxBus_init();
}
//-----------------------------------------------------------------------
IN_ITCM_RAM void mixer_decimateBlock(const uint8_t voiceNr, int16_t* buffer)
{
	uint8_t i;
	float phaseInc = mixer_decimation_rate[voiceNr]*mixer_decimation_rate[6];
	for(i=0;i<OUTPUT_DMA_SIZE;i++)
	{
		mixer_decimation_cnt[voiceNr] += phaseInc;
		if(mixer_decimation_cnt[voiceNr] >= 1.f)
		{
			mixer_decimation_cnt[voiceNr] -= 1.f;
			mixer_voice_samples[voiceNr] = buffer[i];

		}
		buffer[i] = mixer_voice_samples[voiceNr];
	}
}
//#pragma GCC push_options
//#pragma GCC optimize ("O0")
//-----------------------------------------------------------------------
/*IN_ITCM_RAM*/ uint8_t mixer_checkOutJackAvailable(uint8_t dest)
{
	//read input pins
	//TODO move pin reading in low priority idle loop
	/*
	 #define switch_l1_Pin GPIO_PIN_6
#define switch_l1_GPIO_Port GPIOD
#define switch_r1_Pin GPIO_PIN_7
#define switch_r1_GPIO_Port GPIOD
#define switch_l2_Pin GPIO_PIN_3
#define switch_l2_GPIO_Port GPIOB
#define switch_r2_Pin GPIO_PIN_4
#define switch_r2_GPIO_Port GPIOB
	 */


	//return MIXER_ROUTING_DAC2_STEREO;

	switch(dest)
	{
		case MIXER_ROUTING_FX_BUS:
			return MIXER_ROUTING_FX_BUS;
			break;

		case MIXER_ROUTING_DAC1_STEREO:
			if(r1_Available && l1_Available) {
				return MIXER_ROUTING_DAC1_STEREO;
			} else
			if (r2_Available && l2_Available) {
				return MIXER_ROUTING_DAC2_STEREO;
			}
			else if(r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			}else if(l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			} else
			if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			}else
			if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			}
			break;

		case MIXER_ROUTING_DAC2_STEREO:

			if (r2_Available && l2_Available) {
				return MIXER_ROUTING_DAC2_STEREO;
			} else
			if(r1_Available && l1_Available) {
				return MIXER_ROUTING_DAC1_STEREO;
			}
			else
			if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			}else
			if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			}else if(r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			}else if(l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			}
			break;

		case MIXER_ROUTING_DAC1_L:
			if(l1_Available) {
				return dest;
			} else if (r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			} else if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			} else if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			}
			break;

		case MIXER_ROUTING_DAC1_R:
			if(r1_Available) {
				return dest;
			} else if (l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			} else if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			} else if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			}
			break;

		case MIXER_ROUTING_DAC2_L:
			if(l2_Available) {
				return dest;
			} else if (r2_Available) {
				return MIXER_ROUTING_DAC2_R;
			} else if (l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			} else if (r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			}
			break;

		case MIXER_ROUTING_DAC2_R:
			if(r2_Available) {
				return dest;
			} else if (l2_Available) {
				return MIXER_ROUTING_DAC2_L;
			} else if (r1_Available) {
				return MIXER_ROUTING_DAC1_R;
			} else if (l1_Available) {
				return MIXER_ROUTING_DAC1_L;
			}
			break;
	}
	return dest;
}

//-----------------------------------------------------------------------
#if 0
static inline void mixer_moveDataToOutput(uint8_t dest, const float panL, const float panR, int16_t* data,int16_t* outL,int16_t* outR,int16_t* outL2, int16_t* outR2)
{
	//check if a cable is in the selected out
	dest = mixer_checkOutJackAvailable(dest);

	uint8_t i;
	switch(dest)
	{

	case MIXER_ROUTING_DAC1_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL2 = data[i] * panL;
			outL2 += 2;

			*outR2 = data[i] * panR;
			outR2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL = data[i] * panL;
			outL += 2;

			*outR = data[i] * panR;
			outR += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL2 = data[i];
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR2 = data[i];
			outR2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL = data[i];
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR = data[i];
			outR += 2;
		}
		break;
	}
}
#endif
//-----------------------------------------------------------------------
#if 0
static float fastTanh(float var)
{
   if(var < -1.95f)     return -1.0f;
   else if(var > 1.95f) return  1.0f;
   else					return  4.15f*var/(4.29f+var*var);
}




double ecc83(float x, float v)
{
	/*
	double input = x -0.05*v*x +0.01*v;//(h100f(x -0.05*v*x +0.01*v))*(1.0+0.55*v) +0.55*v;
	input += 1.0; // [0:2]
	input *= 64.0;
	uint8_t itg = input;
	float frac = input-itg;
	double val;
	if(itg<127)
		val = (1.0-frac) * ecc83Table[itg] + frac * ecc83Table[itg+1];
	else val = ecc83Table[127];
	val = val *(1.0+0.55*v) +0.55*v;
	//if(val>0.999999)val =0.999999;
	return val;
	*/
}

void mixer_saturateOutputs(float gain,  int16_t* outL,int16_t* outR,int16_t* outL2, int16_t* outR2)
{
    gain = 1+gain*2;
    //float v = gain
	for(uint32_t i=0;i<OUTPUT_DMA_SIZE;i++)
	{
		/*
		*outR = __SSAT((int32_t)(ecc83(*outR/32768.0f, gain) * 32768.0f), 16); ;
		outR += 2;

		*outL =  __SSAT((int32_t)(ecc83(*outL/32768.0f, gain) * 32768.0f), 16); ;
		outL += 2;
		*/

		*outR2 =  __SSAT((int32_t)(ecc83(*outR2/32768.0f*gain, 1.) * 32768.0f), 16); ;
		outR2 += 2;

		*outL2 =  __SSAT((int32_t)(ecc83(*outL2/32768.0f*gain, 1.) * 32768.0f), 16); ;
		outL2 += 2;
	}
}
#endif
//-----------------------------------------------------------------------
IN_ITCM_RAM static inline void mixer_addDataToOutputFilteredPan(uint8_t dest, const float pan,  int16_t* data,int32_t* outL,int32_t* outR,int32_t* outL2, int32_t* outR2, int16_t *fxBuffer, OnePoleFilter* filter)
{


	//check if a cable is in the selected out
	dest = mixer_checkOutJackAvailable(dest);
	//assert_param(dest <= MIXER_ROUTING_DAC2_R);

	//TODO may be possible to optimize here using both halfwords of add for stereo mixing
	uint8_t i;
	switch(dest)
	{

	case MIXER_ROUTING_FX_BUS:
#if 0
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			fxBuffer[i] = __QADD16(fxBuffer[i],data[i]) & 0xFFFF;
		}
#else
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{

			//----
			float mixPan 	= calcOnePole(filter, pan);
			float panL = sqrtf((127-mixPan)/127.f);
			float panR = sqrtf(mixPan/127.f);
			//----
			int32_t y;
			int32_t x;
			y = *((int32_t*)fxBuffer);
			x = __PKHBT( (int16_t)(data[i] * panL), (int16_t)(data[i] * panR), 16);

			*((int32_t*)fxBuffer) = __QADD16(x, y);
			fxBuffer += 2;
		}
#endif
		break;

	case MIXER_ROUTING_DAC1_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			//----
			float mixPan 	= calcOnePole(filter, pan);
			float panL = sqrtf((127-mixPan)/127.f);
			float panR = sqrtf(mixPan/127.f);
			//----
			outL2[0] += (int16_t)(data[i] * panL);
			outL2[1] += (int16_t)(data[i] * panR);
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			//----
			float mixPan 	= calcOnePole(filter, pan);
			float panL = sqrtf((127-mixPan)/127.f);
			float panR = sqrtf(mixPan/127.f);
			//----
			outL[0] += (int16_t)(data[i] * panL);
			outL[1] += (int16_t)(data[i] * panR);
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{

			*outL2 += data[i];
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR2 += data[i];
			outR2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL += data[i];
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR += data[i];
			outR += 2;
		}
		break;

	}
}
//-----------------------------------------------------------------------
IN_ITCM_RAM static inline void mixer_addDataToOutput(uint8_t dest, const float panL, const float panR,  int16_t* data,int32_t* outL,int32_t* outR,int32_t* outL2, int32_t* outR2, int16_t *fxBuffer)
{
	//check if a cable is in the selected out
	dest = mixer_checkOutJackAvailable(dest);
	//assert_param(dest <= MIXER_ROUTING_DAC2_R);

	//TODO may be possible to optimize here using both halfwords of add for stereo mixing
	uint8_t i;
	switch(dest)
	{

	case MIXER_ROUTING_FX_BUS:
#if 0
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			fxBuffer[i] = __QADD16(fxBuffer[i],data[i]) & 0xFFFF;
		}
#else
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			int32_t y;
			int32_t x;
			y = *((int32_t*)fxBuffer);
			x = __PKHBT( (int16_t)(data[i] * panL), (int16_t)(data[i] * panR), 16);

			*((int32_t*)fxBuffer) = __QADD16(x, y);
			fxBuffer += 2;
		}
#endif
		break;

	case MIXER_ROUTING_DAC1_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			outL2[0] += (int16_t)(data[i] * panL);
			outL2[1] += (int16_t)(data[i] * panR);
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_STEREO:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			outL[0] += (int16_t)(data[i] * panL);
			outL[1] += (int16_t)(data[i] * panR);
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL2 += data[i];
			outL2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC1_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR2 += data[i];
			outR2 += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_L:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outL += data[i];
			outL += 2;
		}
		break;
	case MIXER_ROUTING_DAC2_R:
		for(i=0;i<OUTPUT_DMA_SIZE;i++)
		{
			*outR += data[i];
			outR += 2;
		}
		break;

	}
}
//#pragma GCC pop_options
//-----------------------------------------------------------------------
/* Output limiter, one per DAC (L and R linked, to keep the stereo image).
 * Up to full scale the mix passes unchanged, so anything that did not clip
 * before sounds exactly as before. When the sum of the voices and the FX
 * return gets louder, the gain drops at once so that no sample exceeds full
 * scale (no clipping), and then recovers smoothly. */
#define MIXER_LIMITER_THRESHOLD	32768.f
//gain recovery per sample: 1-exp(-1/(0.1s * REAL_FS)), i.e. ~100 ms time constant
#define MIXER_LIMITER_RELEASE	0.000227f

INDTCM float mixer_limiterGain[2] = {1.f, 1.f};

IN_ITCM_RAM static void mixer_limitBlock(float* gainState, const int32_t* in, int16_t* out)
{
	float gain = *gainState;
	for(uint32_t i=0;i<OUTPUT_DMA_SIZE*2;i+=2)
	{
		const int32_t l = in[i];
		const int32_t r = in[i+1];
		const int32_t absL = l < 0 ? -l : l;
		const int32_t absR = r < 0 ? -r : r;
		const int32_t peak = absL > absR ? absL : absR;

		//instant attack: scale this sample down to full scale
		if(peak * gain > MIXER_LIMITER_THRESHOLD)
		{
			gain = MIXER_LIMITER_THRESHOLD / peak;
		}

		if(gain < 1.f)
		{
			out[i]   = __SSAT((int32_t)(l * gain), 16);
			out[i+1] = __SSAT((int32_t)(r * gain), 16);

			gain += (1.f - gain) * MIXER_LIMITER_RELEASE;
			//snap to exactly 1 (a step of < 0.01 dB): in float the release stalls
			//at ~0.99987 and would never get back to a bit-exact pass-through
			if(gain > 0.999f) gain = 1.f;
		}
		else
		{
			out[i]   = __SSAT(l, 16);
			out[i+1] = __SSAT(r, 16);
		}
	}
	*gainState = gain;
}
//-----------------------------------------------------------------------
//the voices in mixing order, index = voice slot 0-5
VoiceCommon* const voiceCommon[6] =
{
	(VoiceCommon*)&voiceArray[0],
	(VoiceCommon*)&voiceArray[1],
	(VoiceCommon*)&voiceArray[2],
	(VoiceCommon*)&snareVoice,
	(VoiceCommon*)&cymbalVoice,
	(VoiceCommon*)&hatVoice,
};
//-----------------------------------------------------------------------
//engine functions of the voice in slot v
IN_ITCM_RAM static inline void mixer_calcVoiceAsync(const uint8_t v, VoiceCommon* voice)
{
	switch(v)
	{
	case 3:		Snare_calcAsync((SnareVoice*)voice);	break;
	case 4:		Cymbal_calcAsync((CymbalVoice*)voice);	break;
	case 5:		HiHat_calcAsync((HiHatVoice*)voice);	break;
	default:	calcDrumVoiceAsync((DrumVoice*)voice);	break;
	}
}
IN_ITCM_RAM static inline void mixer_calcVoiceSyncBlock(const uint8_t v, VoiceCommon* voice, int16_t* buf)
{
	switch(v)
	{
	case 3:		Snare_calcSyncBlock((SnareVoice*)voice, buf, OUTPUT_DMA_SIZE);		break;
	case 4:		Cymbal_calcSyncBlock((CymbalVoice*)voice, buf, OUTPUT_DMA_SIZE);	break;
	case 5:		HiHat_calcSyncBlock((HiHatVoice*)voice, buf, OUTPUT_DMA_SIZE);		break;
	default:	calcDrumVoiceSyncBlock((DrumVoice*)voice, buf, OUTPUT_DMA_SIZE);		break;
	}
}
//-----------------------------------------------------------------------
IN_ITCM_RAM void mixer_calcNextSampleBlock(int16_t* output,int16_t* output2)
{

	r2_Available = (switch_r2_GPIO_Port->IDR & switch_r2_Pin)>0;
	l1_Available = (switch_l1_GPIO_Port->IDR & switch_l1_Pin)>0;
	l2_Available = (switch_l2_GPIO_Port->IDR & switch_l2_Pin)>0;
	r1_Available = (switch_r1_GPIO_Port->IDR & switch_r1_Pin)>0;

	modNode_resetTargets();
	//re assign velocity modulation
	modNode_reassignVeloMod();

	//calc and dispatch LFO
	uint8_t v;
	for(v=0;v<6;v++)
	{
		lfo_dispatchNextValue(&voiceCommon[v]->lfo);
	}

	//update filter frequencies
	for(v=0;v<6;v++)
	{
		SVF_recalcFreq(&voiceCommon[v]->filter);
	}

	//--- Calc async -----
	for(v=0;v<6;v++)
	{
		mixer_calcVoiceAsync(v, voiceCommon[v]);
	}

	//calculate trigger io phase
	//trigger_tickPhaseCounter();

	//an array to store intermediate voice samples
	//befor output distribution
	int16_t sampleData[OUTPUT_DMA_SIZE];
	int16_t fxBusData[OUTPUT_DMA_SIZE*2];

	const uint8_t pos = 0;
#if CPU_METER
	uint32_t meterStart;
#endif

	//32-bit mix buffers, interleaved L/R like the DMA buffers (see mixer.h)
	int32_t mix[OUTPUT_DMA_SIZE*2];
	int32_t mix2[OUTPUT_DMA_SIZE*2];

	//Clear is necessary, otherwise we keep trash data on non routed outputs
	memset(mix, 0, sizeof(mix));
	memset(mix2, 0, sizeof(mix2));

	bufferTool_clearBuffer(fxBusData,OUTPUT_DMA_SIZE*2);

	//calc the voices, decimate them and add them to the selected outputs
	for(v=0;v<6;v++)
	{
		VoiceCommon* const voice = voiceCommon[v];
#if CPU_METER
		meterStart = DWT->CYCCNT;
#endif
		mixer_calcVoiceSyncBlock(v, voice, sampleData);
#if CPU_METER
		cpuMeter_section(CPU_SECTION_VOICE1+v, meterStart);
#endif
		//decimate voice
		mixer_decimateBlock(v,sampleData);
		//copy to selected dma buffer
#if USE_SQRT_LUT
		const float panL = squareRootLut[127-voice->pan];
		const float panR = squareRootLut[voice->pan];
#elif USE_TRIGGERED_PAN
		const float panL = voice->triggeredPanL;
		const float panR = voice->triggeredPanR;
#else
		const float panL = sqrtf((127-voice->pan)/127.f);
		const float panR = sqrtf(voice->pan/127.f);
#endif
		mixer_addDataToOutput(mixer_audioRouting[v], panL, panR, sampleData,&mix[pos],&mix[pos+1],&mix2[pos],&mix2[pos+1], fxBusData);
	}

#if CPU_METER
	//simulate kits with more FM voices: render the cymbal engine again (into the
	//no longer needed sampleData). This also advances the cymbal state faster.
	for(uint8_t i=0;i<cpuMeter.stressFm;i++)
	{
		Cymbal_calcSyncBlock(&cymbalVoice, sampleData,OUTPUT_DMA_SIZE);
	}
#endif
#if CPU_METER
	meterStart = DWT->CYCCNT;
#endif
	FxBus_process(fxBusData,  &mix[pos],&mix[pos+1],&mix2[pos],&mix2[pos+1], OUTPUT_DMA_SIZE);
#if CPU_METER
	cpuMeter_section(CPU_SECTION_FX, meterStart);
#endif

	//bring the sums back into the 16-bit range of the DACs
	mixer_limitBlock(&mixer_limiterGain[0], mix, output);
	mixer_limitBlock(&mixer_limiterGain[1], mix2, output2);
/*
	if(activeParameters[UNUSED_PAR_GLO_SAT])
	{
		mixer_saturateOutputs(activeParameters[UNUSED_PAR_GLO_SAT]/127.f, &mix[pos],&mix[pos+1],&mix2[pos],&mix2[pos+1]);
	}
	*/


}
