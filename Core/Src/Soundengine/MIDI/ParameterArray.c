/*
 * ParameterArray.c
 *
 *  Created on: 06.01.2013
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */



#include "Soundengine/MIDI/ParameterArray.h"
#include "Soundengine/DSPAudio/VoiceSlot.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/MIDI/MidiParser.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/mixer.h"
#include "Soundengine/FxBus.h"
#include "Soundengine/MIDI/MidiMessages.h"
#include "Soundengine/MIDI/MidiParser.h"
#include "frontPanelParser_ui.h"
#include "menu/menu.h"

 Parameter parameterArray[END_OF_SOUND_PARAMETERS];
//the parameter numbers from the AVR/Frontpanel
//####################################################################
 IN_ITCM_RAM void paramArray_setParameter(uint16_t idx, ptrValue newValue)
{

#if 1
	Parameter const *p = &parameterArray[idx];

	// **PATROT avoid setting this if it's not set to something good
	if(!p->ptr)
		return;

	MidiMsg msg;
	uint8_t val = activeParameters[idx];

	if(idx < 128)
	{
		/* comes from cortex midi handler
		 * stupid offset!
		 * TODO fix this stupid offset			 *
		 * 		frontParser_midiMsg.data1 += 1;

				//because hh slope on front is 127 and on cortex is 0 wrap data1 at 127
				frontParser_midiMsg.data1 &= 0x7f;
		 *
		 */
		msg.status = MIDI_CC;
		msg.data1 = idx;
		msg.data2 = val;

		msg.data1 += 1;
		msg.data1 &= 0x7f;
		midiParser_ccHandler(msg,0, 0, 0.f);


	}
	else
	{
		msg.status = CC_2;
		msg.data1 = idx-128; //stupid offset only in CC not CC2
		msg.data2 = val;

		msg.data1 &= 0x7f;
		midiParser_ccHandler(msg,0, 0, 0.f);

	}
#else
	// **PATROT we don't want to set any that have a null ptr
	if(idx>=END_OF_SOUND_PARAMETERS || parameterArray[idx].ptr==0) return;

	switch(parameterArray[idx].type)
	{
	case TYPE_UINT8:
		*((uint8_t*)parameterArray[idx].ptr) = newValue.itg;
		break;

	case TYPE_UINT32:
		*((uint32_t*)parameterArray[idx].ptr) = newValue.itg;
			break;

	case TYPE_FLT:
		*((float*)parameterArray[idx].ptr) 	= newValue.flt;
		break;

	case TYPE_SPECIAL_F: //freq
		*((float*)parameterArray[idx].ptr) 	= newValue.flt;
		break;
	case TYPE_SPECIAL_P: //Pan

		break;

	default:
		break;

	}
#endif
}
//---------------------------------------------------------------------
void parameterArray_init()
{

	parameterArray[PAR_OSC_WAVE_DRUM1].ptr 	= &voiceSlots[0].u.drum.osc.waveform;
	parameterArray[PAR_OSC_WAVE_DRUM1].type = TYPE_UINT8;

	parameterArray[PAR_OSC_WAVE_DRUM2].ptr 	= &voiceSlots[1].u.drum.osc.waveform;
	parameterArray[PAR_OSC_WAVE_DRUM2].type = TYPE_UINT8;

	parameterArray[PAR_OSC_WAVE_DRUM3].ptr 	= &voiceSlots[2].u.drum.osc.waveform;
	parameterArray[PAR_OSC_WAVE_DRUM3].type = TYPE_UINT8;

	parameterArray[PAR_OSC_WAVE_SNARE].ptr 	= &voiceSlots[3].u.snare.osc.waveform;;
	parameterArray[PAR_OSC_WAVE_SNARE].type = TYPE_UINT8;

	parameterArray[PAR_WAVE1_CYM].ptr 		= &voiceSlots[4].u.cymbal.osc.waveform;
	parameterArray[PAR_WAVE1_CYM].type 		= TYPE_UINT8;

	parameterArray[PAR_WAVE1_HH].ptr 		=  &voiceSlots[5].u.hat.osc.waveform;
	parameterArray[PAR_WAVE1_HH].type 		= TYPE_UINT8;


	parameterArray[PAR_COARSE1].ptr 		= &voiceSlots[0].u.drum.osc.modNodeValue;
	parameterArray[PAR_COARSE1].type 		= TYPE_SPECIAL_F;

	parameterArray[PAR_FINE1].ptr 			= &voiceSlots[0].u.drum.osc.modNodeValue;
	parameterArray[PAR_FINE1].type 			= TYPE_SPECIAL_F;

	parameterArray[PAR_COARSE2].ptr 		= &voiceSlots[1].u.drum.osc.modNodeValue;
	parameterArray[PAR_COARSE2].type 		= TYPE_SPECIAL_F;

	parameterArray[PAR_FINE2].ptr 			= &voiceSlots[1].u.drum.osc.modNodeValue;
	parameterArray[PAR_FINE2].type 			= TYPE_SPECIAL_F;

	parameterArray[PAR_COARSE3].ptr 		= &voiceSlots[2].u.drum.osc.modNodeValue;
	parameterArray[PAR_COARSE3].type 		= TYPE_SPECIAL_F;

	parameterArray[PAR_FINE3].ptr 			= &voiceSlots[2].u.drum.osc.modNodeValue;
	parameterArray[PAR_FINE3].type 			= TYPE_SPECIAL_F;

	parameterArray[PAR_COARSE4].ptr 		= &voiceSlots[3].u.snare.osc.modNodeValue;
	parameterArray[PAR_COARSE4].type 		= TYPE_SPECIAL_F;

	parameterArray[PAR_FINE4].ptr 			= &voiceSlots[3].u.snare.osc.modNodeValue;
	parameterArray[PAR_FINE4].type 			= TYPE_SPECIAL_F;

	parameterArray[PAR_COARSE5].ptr 		= &voiceSlots[4].u.cymbal.osc.modNodeValue;
	parameterArray[PAR_COARSE5].type 		= TYPE_SPECIAL_F;

	parameterArray[PAR_FINE5].ptr 			= &voiceSlots[4].u.cymbal.osc.modNodeValue;
	parameterArray[PAR_FINE5].type		 	= TYPE_SPECIAL_F;

	parameterArray[PAR_COARSE6].ptr 		= &voiceSlots[5].u.hat.osc.modNodeValue;
	parameterArray[PAR_COARSE6].type 		= TYPE_SPECIAL_F;

	parameterArray[PAR_FINE6].ptr 			= &voiceSlots[5].u.hat.osc.modNodeValue;
	parameterArray[PAR_FINE6].type 			= TYPE_SPECIAL_F;


	parameterArray[PAR_MOD_WAVE_DRUM1].ptr 	= &voiceSlots[0].u.drum.modOsc.waveform;
	parameterArray[PAR_MOD_WAVE_DRUM1].type = TYPE_UINT8;

	parameterArray[PAR_MOD_WAVE_DRUM2].ptr 	= &voiceSlots[1].u.drum.modOsc.waveform;
	parameterArray[PAR_MOD_WAVE_DRUM2].type = TYPE_UINT8;

	parameterArray[PAR_MOD_WAVE_DRUM3].ptr 	= &voiceSlots[2].u.drum.modOsc.waveform;
	parameterArray[PAR_MOD_WAVE_DRUM3].type = TYPE_UINT8;



	parameterArray[PAR_WAVE2_CYM].ptr 		= &voiceSlots[4].u.cymbal.modOsc.waveform;
	parameterArray[PAR_WAVE2_CYM].type 		= TYPE_UINT8;

	parameterArray[PAR_WAVE3_CYM].ptr 		= &voiceSlots[4].u.cymbal.modOsc2.waveform;;
	parameterArray[PAR_WAVE3_CYM].type 		= TYPE_UINT8;

	parameterArray[PAR_WAVE2_HH].ptr 		= &voiceSlots[5].u.hat.modOsc.waveform;
	parameterArray[PAR_WAVE2_HH].type 		= TYPE_UINT8;

	parameterArray[PAR_WAVE3_HH].ptr 		= &voiceSlots[5].u.hat.modOsc2.waveform;
	parameterArray[PAR_WAVE3_HH].type 		= TYPE_UINT8;

	parameterArray[PAR_NOISE_FREQ1].ptr 	= &voiceSlots[3].u.snare.noiseOsc.modNodeValue;
	parameterArray[PAR_NOISE_FREQ1].type 	= TYPE_SPECIAL_F;
	parameterArray[PAR_MIX1].ptr 			= &voiceSlots[3].u.snare.mix;
	parameterArray[PAR_MIX1].type 			= TYPE_FLT;

	parameterArray[PAR_MOD_OSC_F1_CYM].ptr 	= &voiceSlots[4].u.cymbal.modOsc.modNodeValue;
	parameterArray[PAR_MOD_OSC_F1_CYM].type = TYPE_SPECIAL_F;

	parameterArray[PAR_MOD_OSC_F2_CYM].ptr 	= &voiceSlots[4].u.cymbal.modOsc2.modNodeValue;
	parameterArray[PAR_MOD_OSC_F2_CYM].type = TYPE_SPECIAL_F;		//70

	parameterArray[PAR_MOD_OSC_GAIN1_CYM].ptr 		= &voiceSlots[4].u.cymbal.fmModAmount1;
	parameterArray[PAR_MOD_OSC_GAIN1_CYM].type 		= TYPE_FLT;

	parameterArray[PAR_MOD_OSC_GAIN2_CYM].ptr 		= &voiceSlots[4].u.cymbal.fmModAmount2;
	parameterArray[PAR_MOD_OSC_GAIN2_CYM].type		= TYPE_FLT;

	parameterArray[PAR_MOD_OSC_F1].ptr 		= &voiceSlots[5].u.hat.modOsc.modNodeValue;
	parameterArray[PAR_MOD_OSC_F1].type 	= TYPE_SPECIAL_F;

	parameterArray[PAR_MOD_OSC_F2].ptr 		= &voiceSlots[5].u.hat.modOsc2.modNodeValue;;
	parameterArray[PAR_MOD_OSC_F2].type 	= TYPE_SPECIAL_F;

	parameterArray[PAR_MOD_OSC_GAIN1].ptr 	= &voiceSlots[5].u.hat.fmModAmount1;
	parameterArray[PAR_MOD_OSC_GAIN1].type 	= TYPE_FLT;

	parameterArray[PAR_MOD_OSC_GAIN2].ptr 	= &voiceSlots[5].u.hat.fmModAmount2;
	parameterArray[PAR_MOD_OSC_GAIN2].type 	= TYPE_FLT;

	parameterArray[PAR_FILTER_FREQ_1].ptr 	= &voiceSlots[0].u.drum.filter.f;
	parameterArray[PAR_FILTER_FREQ_1].type 	= TYPE_FLT;//TYPE_SPECIAL_FILTER_F;

	parameterArray[PAR_FILTER_FREQ_2].ptr 	= &voiceSlots[1].u.drum.filter.f;
	parameterArray[PAR_FILTER_FREQ_2].type 	= TYPE_FLT;//TYPE_SPECIAL_FILTER_F;

	parameterArray[PAR_FILTER_FREQ_3].ptr 	= &voiceSlots[2].u.drum.filter.f;
	parameterArray[PAR_FILTER_FREQ_3].type 	= TYPE_FLT;//TYPE_SPECIAL_FILTER_F;

	parameterArray[PAR_FILTER_FREQ_4].ptr 	= &voiceSlots[3].u.snare.filter.f;
	parameterArray[PAR_FILTER_FREQ_4].type 	= TYPE_FLT;//TYPE_SPECIAL_FILTER_F;

	parameterArray[PAR_FILTER_FREQ_5].ptr 	= &voiceSlots[4].u.cymbal.filter.f;
	parameterArray[PAR_FILTER_FREQ_5].type 	= TYPE_FLT;//TYPE_SPECIAL_FILTER_F;

	parameterArray[PAR_FILTER_FREQ_6].ptr 	= &voiceSlots[5].u.hat.filter.f;
	parameterArray[PAR_FILTER_FREQ_6].type 	= TYPE_FLT;//TYPE_SPECIAL_FILTER_F;

	parameterArray[PAR_RESO_1].ptr 			= &voiceSlots[0].u.drum.filter.q;
	parameterArray[PAR_RESO_1].type 		= TYPE_FLT;

	parameterArray[PAR_RESO_2].ptr 			= &voiceSlots[1].u.drum.filter.q;
	parameterArray[PAR_RESO_2].type 		= TYPE_FLT;

	parameterArray[PAR_RESO_3].ptr 			= &voiceSlots[2].u.drum.filter.q;
	parameterArray[PAR_RESO_3].type 		= TYPE_FLT;

	parameterArray[PAR_RESO_4].ptr 			= &voiceSlots[3].u.snare.filter.q;
	parameterArray[PAR_RESO_4].type 		= TYPE_FLT;


	parameterArray[PAR_RESO_5].ptr 			= &voiceSlots[4].u.cymbal.filter.q;
	parameterArray[PAR_RESO_5].type 		= TYPE_FLT;

	parameterArray[PAR_RESO_6].ptr 			= &voiceSlots[5].u.hat.filter.q;
	parameterArray[PAR_RESO_6].type 		= TYPE_FLT;

	parameterArray[PAR_VELOA1].ptr 			= &voiceSlots[0].u.drum.oscVolEg.attack;
	parameterArray[PAR_VELOA1].type 		= TYPE_FLT;

	parameterArray[PAR_VELOD1].ptr 			= &voiceSlots[0].u.drum.oscVolEg.decay;
	parameterArray[PAR_VELOD1].type 		= TYPE_FLT;

	parameterArray[PAR_VELOA2].ptr 			= &voiceSlots[1].u.drum.oscVolEg.attack;
	parameterArray[PAR_VELOA2].type 		= TYPE_FLT;

	parameterArray[PAR_VELOD2].ptr 			= &voiceSlots[1].u.drum.oscVolEg.decay;
	parameterArray[PAR_VELOD2].type 		= TYPE_FLT;

	parameterArray[PAR_VELOA3].ptr 			= &voiceSlots[2].u.drum.oscVolEg.attack;
	parameterArray[PAR_VELOA3].type 		= TYPE_FLT;

	parameterArray[PAR_VELOD3].ptr 			= &voiceSlots[2].u.drum.oscVolEg.decay;
	parameterArray[PAR_VELOD3].type 		= TYPE_FLT;

	parameterArray[PAR_VELOA4].ptr 			= &voiceSlots[3].u.snare.oscVolEg.attack;
	parameterArray[PAR_VELOA4].type 		= TYPE_FLT;

	parameterArray[PAR_VELOD4].ptr 			= &voiceSlots[3].u.snare.oscVolEg.decay;
	parameterArray[PAR_VELOD4].type 		= TYPE_FLT;

	parameterArray[PAR_VELOA5].ptr 			= &voiceSlots[4].u.cymbal.oscVolEg.attack;
	parameterArray[PAR_VELOA5].type 		= TYPE_FLT;

	parameterArray[PAR_VELOD5].ptr 			= &voiceSlots[4].u.cymbal.oscVolEg.decay;
	parameterArray[PAR_VELOD5].type 		= TYPE_FLT;

	parameterArray[PAR_VELOA6].ptr 			= &voiceSlots[5].u.hat.oscVolEg.attack;
	parameterArray[PAR_VELOA6].type 		= TYPE_FLT;

	parameterArray[PAR_VELOD6_CLOSED].ptr 	= &voiceSlots[5].decay[0];
	parameterArray[PAR_VELOD6_CLOSED].type 	= TYPE_FLT;

	parameterArray[PAR_VELOD6_OPEN].ptr 	= &voiceSlots[5].decay[1];
	parameterArray[PAR_VELOD6_OPEN].type 	= TYPE_FLT;

	parameterArray[PAR_VOL_SLOPE1].ptr 		= &voiceSlots[0].u.drum.oscVolEg.slope;
	parameterArray[PAR_VOL_SLOPE1].type 	= TYPE_FLT;

	parameterArray[PAR_VOL_SLOPE2].ptr 		= &voiceSlots[1].u.drum.oscVolEg.slope;
	parameterArray[PAR_VOL_SLOPE2].type 	= TYPE_FLT;

	parameterArray[PAR_VOL_SLOPE3].ptr 		= &voiceSlots[2].u.drum.oscVolEg.slope;
	parameterArray[PAR_VOL_SLOPE3].type 	= TYPE_FLT;

	parameterArray[PAR_VOL_SLOPE4].ptr 		= &voiceSlots[3].u.snare.oscVolEg.slope;
	parameterArray[PAR_VOL_SLOPE4].type 	= TYPE_FLT;

	parameterArray[PAR_VOL_SLOPE5].ptr 		= &voiceSlots[4].u.cymbal.oscVolEg.slope;
	parameterArray[PAR_VOL_SLOPE5].type 	= TYPE_FLT;

	parameterArray[PAR_VOL_SLOPE6].ptr 		= &voiceSlots[5].u.hat.oscVolEg.slope;
	parameterArray[PAR_VOL_SLOPE6].type 	= TYPE_FLT;

	parameterArray[PAR_REPEAT4].ptr 		= &voiceSlots[3].u.snare.oscVolEg.repeat;
	parameterArray[PAR_REPEAT4].type 		= TYPE_UINT8;

	parameterArray[PAR_REPEAT5].ptr 		= &voiceSlots[4].u.cymbal.oscVolEg.repeat;
	parameterArray[PAR_REPEAT5].type 		= TYPE_UINT8;

	parameterArray[PAR_MOD_EG1].ptr 		= &voiceSlots[0].u.drum.oscPitchEg.decay;
	parameterArray[PAR_MOD_EG1].type 		= TYPE_FLT;

	parameterArray[PAR_MOD_EG2].ptr 		= &voiceSlots[1].u.drum.oscPitchEg.decay;
	parameterArray[PAR_MOD_EG2].type 		= TYPE_FLT;

	parameterArray[PAR_MOD_EG3].ptr 		= &voiceSlots[2].u.drum.oscPitchEg.decay;
	parameterArray[PAR_MOD_EG3].type 		= TYPE_FLT;

	parameterArray[PAR_MOD_EG4].ptr 		= &voiceSlots[3].u.snare.oscPitchEg.decay;
	parameterArray[PAR_MOD_EG4].type 		= TYPE_FLT;
	parameterArray[PAR_MODAMNT1].ptr 		= &voiceSlots[0].u.drum.egPitchModAmount;
	parameterArray[PAR_MODAMNT1].type 		= TYPE_FLT;
	parameterArray[PAR_MODAMNT2].ptr 		= &voiceSlots[1].u.drum.egPitchModAmount;
	parameterArray[PAR_MODAMNT2].type 		= TYPE_FLT;
	parameterArray[PAR_MODAMNT3].ptr 		= &voiceSlots[2].u.drum.egPitchModAmount;
	parameterArray[PAR_MODAMNT3].type 		= TYPE_FLT;

	parameterArray[PAR_MODAMNT4].ptr 		= &voiceSlots[3].u.snare.egPitchModAmount;
	parameterArray[PAR_MODAMNT4].type 		= TYPE_FLT;

	parameterArray[PAR_PITCH_SLOPE1].ptr 	= &voiceSlots[0].u.drum.oscPitchEg.slope;
	parameterArray[PAR_PITCH_SLOPE1].type 	= TYPE_FLT;

	parameterArray[PAR_PITCH_SLOPE2].ptr 	= &voiceSlots[1].u.drum.oscPitchEg.slope;
	parameterArray[PAR_PITCH_SLOPE2].type 	= TYPE_FLT;

	parameterArray[PAR_PITCH_SLOPE3].ptr 	= &voiceSlots[2].u.drum.oscPitchEg.slope;
	parameterArray[PAR_PITCH_SLOPE3].type 	= TYPE_FLT;

	parameterArray[PAR_PITCH_SLOPE4].ptr 	= &voiceSlots[3].u.snare.oscPitchEg.slope;
	parameterArray[PAR_PITCH_SLOPE4].type 	= TYPE_FLT;

	parameterArray[PAR_FMAMNT1].ptr 		= &voiceSlots[0].u.drum.fmModAmount;
	parameterArray[PAR_FMAMNT1].type 		= TYPE_FLT;

	parameterArray[PAR_FM_FREQ1].ptr 		= &voiceSlots[0].u.drum.modOsc.modNodeValue;
	parameterArray[PAR_FM_FREQ1].type 		= TYPE_SPECIAL_F;//TYPE_UINT32;

	parameterArray[PAR_FMAMNT2].ptr 		= &voiceSlots[1].u.drum.fmModAmount;
	parameterArray[PAR_FMAMNT2].type 		= TYPE_FLT;

	parameterArray[PAR_FM_FREQ2].ptr 		= &voiceSlots[1].u.drum.modOsc.modNodeValue;
	parameterArray[PAR_FM_FREQ2].type 		= TYPE_SPECIAL_F;//TYPE_UINT32;

	parameterArray[PAR_FMAMNT3].ptr 		= &voiceSlots[2].u.drum.fmModAmount;
	parameterArray[PAR_FMAMNT3].type 		= TYPE_FLT;

	parameterArray[PAR_FM_FREQ3].ptr 		= &voiceSlots[2].u.drum.modOsc.modNodeValue;
	parameterArray[PAR_FM_FREQ3].type 		= TYPE_SPECIAL_F;//TYPE_UINT32;


	parameterArray[PAR_VOL1].ptr 			= &voiceSlots[0].u.drum.vol;
	parameterArray[PAR_VOL1].type			= TYPE_FLT;
	parameterArray[PAR_VOL2].ptr 			= &voiceSlots[1].u.drum.vol;
	parameterArray[PAR_VOL2].type			= TYPE_FLT;
	parameterArray[PAR_VOL3].ptr 			= &voiceSlots[2].u.drum.vol;
	parameterArray[PAR_VOL3].type			= TYPE_FLT;
	parameterArray[PAR_VOL4].ptr 			= &voiceSlots[3].u.snare.vol;
	parameterArray[PAR_VOL4].type 			= TYPE_FLT;
	parameterArray[PAR_VOL5].ptr 			= &voiceSlots[4].u.cymbal.vol;
	parameterArray[PAR_VOL5].type 			= TYPE_FLT;
	parameterArray[PAR_VOL6].ptr 			= &voiceSlots[5].u.hat.vol;
	parameterArray[PAR_VOL6].type 			= TYPE_FLT;

	parameterArray[PAR_PAN1].ptr 			= &voiceSlots[0].u.drum.pan;
	parameterArray[PAR_PAN1].type 			= TYPE_UINT8;
	parameterArray[PAR_PAN2].ptr 			= &voiceSlots[1].u.drum.pan;
	parameterArray[PAR_PAN2].type 			= TYPE_UINT8;
	parameterArray[PAR_PAN3].ptr 			= &voiceSlots[2].u.drum.pan;
	parameterArray[PAR_PAN3].type 			= TYPE_UINT8;
	parameterArray[PAR_PAN4].ptr 			= &voiceSlots[3].u.snare.pan;
	parameterArray[PAR_PAN4].type 			= TYPE_UINT8;
	parameterArray[PAR_PAN5].ptr 			= &voiceSlots[4].u.cymbal.pan;
	parameterArray[PAR_PAN5].type 			= TYPE_UINT8;
	parameterArray[PAR_PAN6].ptr 			= &voiceSlots[5].u.hat.pan;
	parameterArray[PAR_PAN6].type 			= TYPE_UINT8;

	parameterArray[PAR_DRIVE1].ptr 			= &voiceSlots[0].u.drum.distortion.shape;
	parameterArray[PAR_DRIVE1].type 		= TYPE_FLT;

	parameterArray[PAR_DRIVE2].ptr 			= &voiceSlots[1].u.drum.distortion.shape;
	parameterArray[PAR_DRIVE2].type 		= TYPE_FLT;

	parameterArray[PAR_DRIVE3].ptr 			= &voiceSlots[2].u.drum.distortion.shape;
	parameterArray[PAR_DRIVE3].type 		= TYPE_FLT;

	parameterArray[PAR_SNARE_DISTORTION].ptr 	= &voiceSlots[3].u.snare.distortion.shape;
	parameterArray[PAR_SNARE_DISTORTION].type 	= TYPE_FLT;

	parameterArray[PAR_CYMBAL_DISTORTION].ptr 	= &voiceSlots[4].u.cymbal.distortion.shape;
	parameterArray[PAR_CYMBAL_DISTORTION].type 	= TYPE_FLT;

	parameterArray[PAR_HAT_DISTORTION].ptr 		= &voiceSlots[5].u.hat.distortion.shape;
	parameterArray[PAR_HAT_DISTORTION].type 	= TYPE_FLT;

	parameterArray[PAR_VOICE_DECIMATION1].ptr 	= &mixer_decimation_rate[0];
	parameterArray[PAR_VOICE_DECIMATION1].type 	= TYPE_FLT;
	parameterArray[PAR_VOICE_DECIMATION2].ptr 	= &mixer_decimation_rate[1];
	parameterArray[PAR_VOICE_DECIMATION2].type 	= TYPE_FLT;
	parameterArray[PAR_VOICE_DECIMATION3].ptr 	= &mixer_decimation_rate[2];
	parameterArray[PAR_VOICE_DECIMATION3].type 	= TYPE_FLT;
	parameterArray[PAR_VOICE_DECIMATION4].ptr 	= &mixer_decimation_rate[3];
	parameterArray[PAR_VOICE_DECIMATION4].type 	= TYPE_FLT;
	parameterArray[PAR_VOICE_DECIMATION5].ptr 	= &mixer_decimation_rate[4];
	parameterArray[PAR_VOICE_DECIMATION5].type 	= TYPE_FLT;
	parameterArray[PAR_VOICE_DECIMATION6].ptr 	= &mixer_decimation_rate[5];
	parameterArray[PAR_VOICE_DECIMATION6].type 	= TYPE_FLT;
	parameterArray[PAR_VOICE_DECIMATION_ALL].ptr= &mixer_decimation_rate[6];
	parameterArray[PAR_VOICE_DECIMATION_ALL].type 	= TYPE_FLT;

	parameterArray[PAR_FREQ_LFO1].ptr 		= &voiceSlots[0].u.drum.lfo.modNodeValue;
	parameterArray[PAR_FREQ_LFO1].type 		= TYPE_SPECIAL_F;//TYPE_FLT;
	parameterArray[PAR_FREQ_LFO2].ptr 		= &voiceSlots[1].u.drum.lfo.modNodeValue;
	parameterArray[PAR_FREQ_LFO2].type 		= TYPE_SPECIAL_F;//TYPE_FLT;
	parameterArray[PAR_FREQ_LFO3].ptr 		= &voiceSlots[2].u.drum.lfo.modNodeValue;
	parameterArray[PAR_FREQ_LFO3].type 		= TYPE_SPECIAL_F;//TYPE_FLT;
	parameterArray[PAR_FREQ_LFO4].ptr 		= &voiceSlots[3].u.snare.lfo.modNodeValue;
	parameterArray[PAR_FREQ_LFO4].type 		= TYPE_SPECIAL_F;//TYPE_FLT;
	parameterArray[PAR_FREQ_LFO5].ptr 		= &voiceSlots[4].u.cymbal.lfo.modNodeValue;
	parameterArray[PAR_FREQ_LFO5].type 		= TYPE_SPECIAL_F;//TYPE_FLT;
	parameterArray[PAR_FREQ_LFO6].ptr 		= &voiceSlots[5].u.hat.lfo.modNodeValue;
	parameterArray[PAR_FREQ_LFO6].type 		= TYPE_SPECIAL_F;//TYPE_FLT;

	parameterArray[PAR_AMOUNT_LFO1].ptr 	= &voiceSlots[0].u.drum.lfo.modTarget.amount;
	parameterArray[PAR_AMOUNT_LFO1].type 	= TYPE_FLT;
	parameterArray[PAR_AMOUNT_LFO2].ptr 	= &voiceSlots[1].u.drum.lfo.modTarget.amount;
	parameterArray[PAR_AMOUNT_LFO2].type 	= TYPE_FLT;
	parameterArray[PAR_AMOUNT_LFO3].ptr 	= &voiceSlots[2].u.drum.lfo.modTarget.amount;
	parameterArray[PAR_AMOUNT_LFO3].type 	= TYPE_FLT;
	parameterArray[PAR_AMOUNT_LFO4].ptr 	= &voiceSlots[3].u.snare.lfo.modTarget.amount;
	parameterArray[PAR_AMOUNT_LFO4].type 	= TYPE_FLT;
	parameterArray[PAR_AMOUNT_LFO5].ptr 	= &voiceSlots[4].u.cymbal.lfo.modTarget.amount;
	parameterArray[PAR_AMOUNT_LFO5].type 	= TYPE_FLT;
	parameterArray[PAR_AMOUNT_LFO6].ptr 	= &voiceSlots[5].u.hat.lfo.modTarget.amount;
	parameterArray[PAR_AMOUNT_LFO6].type 	= TYPE_FLT;
	//######################################
	//######## END OF MIDI DATASIZE ########
	//######## PARAM NR 127 REACHED ########
	//######################################

	parameterArray[PAR_FILTER_DRIVE_1].ptr 	= &voiceSlots[0].u.drum.filter.drive;
	parameterArray[PAR_FILTER_DRIVE_1].type = TYPE_FLT;

	parameterArray[PAR_FILTER_DRIVE_2].ptr 	= &voiceSlots[1].u.drum.filter.drive;
	parameterArray[PAR_FILTER_DRIVE_2].type = TYPE_FLT;

	parameterArray[PAR_FILTER_DRIVE_3].ptr 	= &voiceSlots[2].u.drum.filter.drive;
	parameterArray[PAR_FILTER_DRIVE_3].type = TYPE_FLT;

	parameterArray[PAR_FILTER_DRIVE_4].ptr 	= &voiceSlots[3].u.snare.filter.drive;
	parameterArray[PAR_FILTER_DRIVE_4].type = TYPE_FLT;

	parameterArray[PAR_FILTER_DRIVE_5].ptr 	= &voiceSlots[4].u.cymbal.filter.drive;
	parameterArray[PAR_FILTER_DRIVE_5].type = TYPE_FLT;

	parameterArray[PAR_FILTER_DRIVE_6].ptr 	= &voiceSlots[5].u.hat.filter.drive;
	parameterArray[PAR_FILTER_DRIVE_6].type = TYPE_FLT;

	parameterArray[PAR_MIX_MOD_1].ptr 		= &voiceSlots[0].u.drum.mixOscs;
	parameterArray[PAR_MIX_MOD_1].type 		= TYPE_UINT8;

	parameterArray[PAR_MIX_MOD_2].ptr 		= &voiceSlots[1].u.drum.mixOscs;
	parameterArray[PAR_MIX_MOD_2].type 		= TYPE_UINT8;

	parameterArray[PAR_MIX_MOD_3].ptr 		= &voiceSlots[2].u.drum.mixOscs;
	parameterArray[PAR_MIX_MOD_3].type 		= TYPE_UINT8;



	parameterArray[PAR_VOLUME_MOD_ON_OFF1].ptr 			= &voiceSlots[0].u.drum.volumeMod;
	parameterArray[PAR_VOLUME_MOD_ON_OFF1].type 		= TYPE_UINT8;

	parameterArray[PAR_VOLUME_MOD_ON_OFF2].ptr 			= &voiceSlots[1].u.drum.volumeMod;
	parameterArray[PAR_VOLUME_MOD_ON_OFF2].type		 	= TYPE_UINT8;

	parameterArray[PAR_VOLUME_MOD_ON_OFF3].ptr 			= &voiceSlots[2].u.drum.volumeMod;
	parameterArray[PAR_VOLUME_MOD_ON_OFF3].type 		= TYPE_UINT8;

	parameterArray[PAR_VOLUME_MOD_ON_OFF4].ptr 			= &voiceSlots[3].u.snare.volumeMod;;
	parameterArray[PAR_VOLUME_MOD_ON_OFF4].type 		= TYPE_UINT8;

	parameterArray[PAR_VOLUME_MOD_ON_OFF5].ptr 			= &voiceSlots[4].u.cymbal.volumeMod;
	parameterArray[PAR_VOLUME_MOD_ON_OFF5].type 		= TYPE_UINT8;

	parameterArray[PAR_VOLUME_MOD_ON_OFF6].ptr 			= &voiceSlots[5].u.hat.volumeMod;
	parameterArray[PAR_VOLUME_MOD_ON_OFF6].type 		= TYPE_UINT8;

	parameterArray[PAR_VELO_MOD_AMT_1].ptr 	= &velocityModulators[0].amount;
	parameterArray[PAR_VELO_MOD_AMT_1].type = TYPE_FLT;

	parameterArray[PAR_VELO_MOD_AMT_2].ptr 	= &velocityModulators[1].amount;
	parameterArray[PAR_VELO_MOD_AMT_2].type = TYPE_FLT;

	parameterArray[PAR_VELO_MOD_AMT_3].ptr 	= &velocityModulators[2].amount;
	parameterArray[PAR_VELO_MOD_AMT_3].type = TYPE_FLT;

	parameterArray[PAR_VELO_MOD_AMT_4].ptr 	= &velocityModulators[3].amount;
	parameterArray[PAR_VELO_MOD_AMT_4].type = TYPE_FLT;

	parameterArray[PAR_VELO_MOD_AMT_5].ptr 	= &velocityModulators[4].amount;
	parameterArray[PAR_VELO_MOD_AMT_5].type = TYPE_FLT;

	parameterArray[PAR_VELO_MOD_AMT_6].ptr 	= &velocityModulators[5].amount;
	parameterArray[PAR_VELO_MOD_AMT_6].type = TYPE_FLT;

	parameterArray[PAR_VEL_DEST_1].ptr 		= &velocityModulators[0].destination;
	parameterArray[PAR_VEL_DEST_1].type 	= TYPE_UINT8;

	parameterArray[PAR_VEL_DEST_2].ptr 		= &velocityModulators[1].destination;
	parameterArray[PAR_VEL_DEST_2].type 	= TYPE_UINT8;

	parameterArray[PAR_VEL_DEST_3].ptr 		= &velocityModulators[2].destination;
	parameterArray[PAR_VEL_DEST_3].type 	= TYPE_UINT8;

	parameterArray[PAR_VEL_DEST_4].ptr 		= &velocityModulators[3].destination;
	parameterArray[PAR_VEL_DEST_4].type 	= TYPE_UINT8;

	parameterArray[PAR_VEL_DEST_5].ptr 		= &velocityModulators[4].destination;
	parameterArray[PAR_VEL_DEST_5].type 	= TYPE_UINT8;

	parameterArray[PAR_VEL_DEST_6].ptr 		= &velocityModulators[5].destination;
	parameterArray[PAR_VEL_DEST_6].type 	= TYPE_UINT8;

	parameterArray[PAR_WAVE_LFO1].ptr 		= &voiceSlots[0].u.drum.lfo.waveform;
	parameterArray[PAR_WAVE_LFO1].type = TYPE_UINT8;
	parameterArray[PAR_WAVE_LFO2].ptr 		= &voiceSlots[1].u.drum.lfo.waveform;
	parameterArray[PAR_WAVE_LFO2].type = TYPE_UINT8;
	parameterArray[PAR_WAVE_LFO3].ptr 		= &voiceSlots[2].u.drum.lfo.waveform;
	parameterArray[PAR_WAVE_LFO3].type = TYPE_UINT8;
	parameterArray[PAR_WAVE_LFO4].ptr 		= &voiceSlots[3].u.snare.lfo.waveform;
	parameterArray[PAR_WAVE_LFO4].type = TYPE_UINT8;
	parameterArray[PAR_WAVE_LFO5].ptr 		= &voiceSlots[4].u.cymbal.lfo.waveform;
	parameterArray[PAR_WAVE_LFO5].type = TYPE_UINT8;
	parameterArray[PAR_WAVE_LFO6].ptr 		= &voiceSlots[5].u.hat.lfo.waveform;
	parameterArray[PAR_WAVE_LFO6].type = TYPE_UINT8;

	/*
	PAR_VOICE_LFO1,
	PAR_VOICE_LFO2,
	PAR_VOICE_LFO3,
	PAR_VOICE_LFO4,
	PAR_VOICE_LFO5,
	PAR_VOICE_LFO6,

	PAR_TARGET_LFO1,
	PAR_TARGET_LFO2,
	PAR_TARGET_LFO3,
	PAR_TARGET_LFO4,
	PAR_TARGET_LFO5,
	PAR_TARGET_LFO6,
	*/

	parameterArray[PAR_RETRIGGER_LFO1].ptr 	= &voiceSlots[0].u.drum.lfo.retrigger;
	parameterArray[PAR_RETRIGGER_LFO1].type = TYPE_UINT8;
	parameterArray[PAR_RETRIGGER_LFO2].ptr 	= &voiceSlots[1].u.drum.lfo.retrigger;
	parameterArray[PAR_RETRIGGER_LFO2].type = TYPE_UINT8;
	parameterArray[PAR_RETRIGGER_LFO3].ptr 	= &voiceSlots[2].u.drum.lfo.retrigger;
	parameterArray[PAR_RETRIGGER_LFO3].type = TYPE_UINT8;
	parameterArray[PAR_RETRIGGER_LFO4].ptr 	= &voiceSlots[3].u.snare.lfo.retrigger;
	parameterArray[PAR_RETRIGGER_LFO4].type = TYPE_UINT8;
	parameterArray[PAR_RETRIGGER_LFO5].ptr 	= &voiceSlots[4].u.cymbal.lfo.retrigger;
	parameterArray[PAR_RETRIGGER_LFO5].type = TYPE_UINT8;
	parameterArray[PAR_RETRIGGER_LFO6].ptr 	= &voiceSlots[5].u.hat.lfo.retrigger;
	parameterArray[PAR_RETRIGGER_LFO6].type = TYPE_UINT8;

	parameterArray[PAR_SYNC_LFO1].ptr 		= &voiceSlots[0].u.drum.lfo.sync;
	parameterArray[PAR_SYNC_LFO1].type		= TYPE_UINT8;
	parameterArray[PAR_SYNC_LFO2].ptr 		= &voiceSlots[1].u.drum.lfo.sync;
	parameterArray[PAR_SYNC_LFO2].type 		= TYPE_UINT8;
	parameterArray[PAR_SYNC_LFO3].ptr 		= &voiceSlots[2].u.drum.lfo.sync;
	parameterArray[PAR_SYNC_LFO3].type 		= TYPE_UINT8;
	parameterArray[PAR_SYNC_LFO4].ptr 		= &voiceSlots[3].u.snare.lfo.sync;
	parameterArray[PAR_SYNC_LFO4].type 		= TYPE_UINT8;
	parameterArray[PAR_SYNC_LFO5].ptr 		= &voiceSlots[4].u.cymbal.lfo.sync;
	parameterArray[PAR_SYNC_LFO5].type 		= TYPE_UINT8;
	parameterArray[PAR_SYNC_LFO6].ptr 		= &voiceSlots[5].u.hat.lfo.sync;
	parameterArray[PAR_SYNC_LFO6].type 		= TYPE_UINT8;

	parameterArray[PAR_OFFSET_LFO1].ptr 	= &voiceSlots[0].u.drum.lfo.phaseOffset;
	parameterArray[PAR_OFFSET_LFO1].type 	= TYPE_UINT32;
	parameterArray[PAR_OFFSET_LFO2].ptr 	= &voiceSlots[1].u.drum.lfo.phaseOffset;
	parameterArray[PAR_OFFSET_LFO2].type 	= TYPE_UINT32;
	parameterArray[PAR_OFFSET_LFO3].ptr 	= &voiceSlots[2].u.drum.lfo.phaseOffset;
	parameterArray[PAR_OFFSET_LFO3].type 	= TYPE_UINT32;
	parameterArray[PAR_OFFSET_LFO4].ptr 	= &voiceSlots[3].u.snare.lfo.phaseOffset;
	parameterArray[PAR_OFFSET_LFO4].type 	= TYPE_UINT32;
	parameterArray[PAR_OFFSET_LFO5].ptr 	= &voiceSlots[4].u.cymbal.lfo.phaseOffset;
	parameterArray[PAR_OFFSET_LFO5].type 	= TYPE_UINT32;
	parameterArray[PAR_OFFSET_LFO6].ptr 	= &voiceSlots[5].u.hat.lfo.phaseOffset;
	parameterArray[PAR_OFFSET_LFO6].type 	= TYPE_UINT32;

	parameterArray[PAR_FILTER_TYPE_1].ptr 	= &voiceSlots[0].u.drum.filterType;
	parameterArray[PAR_FILTER_TYPE_1].type 	= TYPE_UINT8;
	parameterArray[PAR_FILTER_TYPE_2].ptr 	= &voiceSlots[1].u.drum.filterType;
	parameterArray[PAR_FILTER_TYPE_2].type 	= TYPE_UINT8;
	parameterArray[PAR_FILTER_TYPE_3].ptr 	= &voiceSlots[2].u.drum.filterType;
	parameterArray[PAR_FILTER_TYPE_3].type 	= TYPE_UINT8;
	parameterArray[PAR_FILTER_TYPE_4].ptr 	= &voiceSlots[3].u.snare.filterType;
	parameterArray[PAR_FILTER_TYPE_4].type 	= TYPE_UINT8;
	parameterArray[PAR_FILTER_TYPE_5].ptr 	= &voiceSlots[4].u.cymbal.filterType;;
	parameterArray[PAR_FILTER_TYPE_5].type 	= TYPE_UINT8;
	parameterArray[PAR_FILTER_TYPE_6].ptr 	= &voiceSlots[5].u.hat.filterType;;
	parameterArray[PAR_FILTER_TYPE_6].type 	= TYPE_UINT8;

	parameterArray[PAR_TRANS1_VOL].ptr 		= &voiceSlots[0].u.drum.transGen.volume;
	parameterArray[PAR_TRANS1_VOL].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS2_VOL].ptr 		= &voiceSlots[1].u.drum.transGen.volume;;
	parameterArray[PAR_TRANS2_VOL].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS3_VOL].ptr 		= &voiceSlots[2].u.drum.transGen.volume;;
	parameterArray[PAR_TRANS3_VOL].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS4_VOL].ptr 		= &voiceSlots[3].u.snare.transGen.volume;
	parameterArray[PAR_TRANS4_VOL].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS5_VOL].ptr 		= &voiceSlots[4].u.cymbal.transGen.volume;
	parameterArray[PAR_TRANS5_VOL].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS6_VOL].ptr 		= &voiceSlots[5].u.hat.transGen.volume;
	parameterArray[PAR_TRANS6_VOL].type 	= TYPE_FLT;



	parameterArray[PAR_TRANS1_WAVE].ptr 	= &voiceSlots[0].u.drum.transGen.waveform;
	parameterArray[PAR_TRANS1_WAVE].type 	= TYPE_UINT8;
	parameterArray[PAR_TRANS2_WAVE].ptr 	= &voiceSlots[1].u.drum.transGen.waveform;;
	parameterArray[PAR_TRANS2_WAVE].type 	= TYPE_UINT8;
	parameterArray[PAR_TRANS3_WAVE].ptr 	= &voiceSlots[2].u.drum.transGen.waveform;;
	parameterArray[PAR_TRANS3_WAVE].type 	= TYPE_UINT8;
	parameterArray[PAR_TRANS4_WAVE].ptr 	= &voiceSlots[3].u.snare.transGen.waveform;
	parameterArray[PAR_TRANS4_WAVE].type 	= TYPE_UINT8;
	parameterArray[PAR_TRANS5_WAVE].ptr 	= &voiceSlots[4].u.cymbal.transGen.waveform;
	parameterArray[PAR_TRANS5_WAVE].type 	= TYPE_UINT8;
	parameterArray[PAR_TRANS6_WAVE].ptr 	= &voiceSlots[5].u.hat.transGen.waveform;
	parameterArray[PAR_TRANS6_WAVE].type 	= TYPE_UINT8;


	parameterArray[PAR_TRANS1_FREQ].ptr 	= &voiceSlots[0].u.drum.transGen.pitch;
	parameterArray[PAR_TRANS1_FREQ].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS2_FREQ].ptr 	= &voiceSlots[1].u.drum.transGen.pitch;
	parameterArray[PAR_TRANS2_FREQ].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS3_FREQ].ptr 	= &voiceSlots[2].u.drum.transGen.pitch;;
	parameterArray[PAR_TRANS3_FREQ].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS4_FREQ].ptr 	= &voiceSlots[3].u.snare.transGen.pitch;
	parameterArray[PAR_TRANS4_FREQ].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS5_FREQ].ptr 	= &voiceSlots[4].u.cymbal.transGen.pitch;
	parameterArray[PAR_TRANS5_FREQ].type 	= TYPE_FLT;
	parameterArray[PAR_TRANS6_FREQ].ptr 	= &voiceSlots[5].u.hat.transGen.pitch;
	parameterArray[PAR_TRANS6_FREQ].type 	= TYPE_FLT;

	parameterArray[PAR_AUDIO_OUT1].ptr 		= &mixer_audioRouting[0];
	parameterArray[PAR_AUDIO_OUT1].type 	= TYPE_UINT8;
	parameterArray[PAR_AUDIO_OUT2].ptr 		= &mixer_audioRouting[1];
	parameterArray[PAR_AUDIO_OUT2].type		= TYPE_UINT8;
	parameterArray[PAR_AUDIO_OUT3].ptr 		= &mixer_audioRouting[2];
	parameterArray[PAR_AUDIO_OUT3].type 	= TYPE_UINT8;
	parameterArray[PAR_AUDIO_OUT4].ptr 		= &mixer_audioRouting[3];
	parameterArray[PAR_AUDIO_OUT4].type 	= TYPE_UINT8;
	parameterArray[PAR_AUDIO_OUT5].ptr 		= &mixer_audioRouting[4];
	parameterArray[PAR_AUDIO_OUT5].type 	= TYPE_UINT8;
	parameterArray[PAR_AUDIO_OUT6].ptr 		= &mixer_audioRouting[5];
	parameterArray[PAR_AUDIO_OUT6].type 	= TYPE_UINT8;

	/*
	parameterArray[PAR_MIDI_NOTE1].ptr 		= &midi_NoteOverride[0];
	parameterArray[PAR_MIDI_NOTE1].type 	= TYPE_UINT8;
	parameterArray[PAR_MIDI_NOTE2].ptr 		= &midi_NoteOverride[1];
	parameterArray[PAR_MIDI_NOTE2].type		= TYPE_UINT8;
	parameterArray[PAR_MIDI_NOTE3].ptr 		= &midi_NoteOverride[2];
	parameterArray[PAR_MIDI_NOTE3].type 	= TYPE_UINT8;
	parameterArray[PAR_MIDI_NOTE4].ptr 		= &midi_NoteOverride[3];
	parameterArray[PAR_MIDI_NOTE4].type 	= TYPE_UINT8;
	parameterArray[PAR_MIDI_NOTE5].ptr 		= &midi_NoteOverride[4];
	parameterArray[PAR_MIDI_NOTE5].type 	= TYPE_UINT8;
	parameterArray[PAR_MIDI_NOTE6].ptr 		= &midi_NoteOverride[5];
	parameterArray[PAR_MIDI_NOTE6].type 	= TYPE_UINT8;
	parameterArray[PAR_MIDI_NOTE7].ptr 		= &midi_NoteOverride[6];
	parameterArray[PAR_MIDI_NOTE7].type 	= TYPE_UINT8;
	*/

	parameterArray[PAR_PWM_1].ptr 			= &voiceSlots[0].u.drum.pwm;
	parameterArray[PAR_PWM_1].type 			= TYPE_FLT;
	parameterArray[PAR_PWM_2].ptr 			= &voiceSlots[1].u.drum.pwm;
	parameterArray[PAR_PWM_2].type 			= TYPE_FLT;
	parameterArray[PAR_PWM_3].ptr 			= &voiceSlots[2].u.drum.pwm;
	parameterArray[PAR_PWM_3].type 			= TYPE_FLT;
	parameterArray[PAR_PWM_4].ptr 			= &voiceSlots[3].u.snare.pwm;
	parameterArray[PAR_PWM_4].type 			= TYPE_FLT;
	parameterArray[PAR_PWM_5].ptr 			= &voiceSlots[4].u.cymbal.pwm;
	parameterArray[PAR_PWM_5].type 			= TYPE_FLT;
	parameterArray[PAR_PWM_6].ptr 			= &voiceSlots[5].u.hat.pwm;
	parameterArray[PAR_PWM_6].type 			= TYPE_FLT;

	parameterArray[PAR_FX_TYPE].ptr 		= &fxBus.fxType;
	parameterArray[PAR_FX_TYPE].type 		= TYPE_UINT8;
	parameterArray[PAR_FX_ROUTING].ptr 		= &fxBus.audioRouting;
	parameterArray[PAR_FX_ROUTING].type 	= TYPE_UINT8;
	parameterArray[PAR_FX_DRY_WET].ptr 		= &fxBus.dryWet;
	parameterArray[PAR_FX_DRY_WET].type 		= TYPE_FLT;

	parameterArray[PAR_FX_DIST_TYPE].ptr 		= &fxBus.distortion.distortionType;
	parameterArray[PAR_FX_DIST_TYPE].type 		= TYPE_UINT8;
	parameterArray[PAR_FX_P1].ptr 		= &fxBus.parameter[0];
	parameterArray[PAR_FX_P1].type 		= TYPE_FLT;
	parameterArray[PAR_FX_P2].ptr 		= &fxBus.parameter[1];
	parameterArray[PAR_FX_P2].type 		= TYPE_FLT;
	parameterArray[PAR_FX_P3].ptr 		= &fxBus.parameter[2];
	parameterArray[PAR_FX_P3].type 		= TYPE_FLT;
	parameterArray[PAR_FX_P4].ptr 		= &fxBus.parameter[3];
	parameterArray[PAR_FX_P4].type 		= TYPE_FLT;

	parameterArray[PAR_FX_RATIO].ptr 		= &fxBus.compressor.ratio;
	parameterArray[PAR_FX_RATIO].type 		= TYPE_UINT8;

	parameterArray[PAR_FX_WAVE].ptr 		= &fxBus.ringmod.osc.waveform;
	parameterArray[PAR_FX_WAVE].type 		= TYPE_UINT8;

	parameterArray[PAR_FX_DELAY_TYPE].ptr 		= &fxBus.delay.delayType;
	parameterArray[PAR_FX_DELAY_TYPE].type 		= TYPE_UINT8;




	//#########################################
	//######## End of sound Parameters ########
	//#########################################
}
