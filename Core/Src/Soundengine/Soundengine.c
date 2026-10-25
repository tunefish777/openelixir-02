/*
 * Soundengine.c
 *
 *  Created on: Mar 24, 2020
 *      Author: julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */
#include "Soundengine/Soundengine.h"
#include "Soundengine/AudioCodecManager.h"
#include "Preset/PresetManager.h"
#include <string.h>
#include "Soundengine/USB/usb_manager.h"
#include "Soundengine/MIDI/Uart.h"
#include "Soundengine/CpuMeter.h"

uint8_t soundengine_calcNextSampleBlock()
{
	if(bCurrentSampleValid!= SAMPLE_VALID)
	{
#if CPU_METER
		const uint32_t meterStart = cpuMeter_dspStart();
#endif
	#if USE_DAC2
		mixer_calcNextSampleBlock((int16_t*)&dma_buffer[bCurrentSampleValid*(OUTPUT_DMA_SIZE*2)],(int16_t*)&dma_buffer2[bCurrentSampleValid*(OUTPUT_DMA_SIZE*2)]);
	#else
		mixer_calcNextSampleBlock(&dma_buffer[bCurrentSampleValid*(OUTPUT_DMA_SIZE*2)],&dma_buffer2[(1-bCurrentSampleValid)*(OUTPUT_DMA_SIZE*2)]);
	#endif
#if CPU_METER
		cpuMeter_dspEnd(meterStart);
#endif
		bCurrentSampleValid = SAMPLE_VALID;

		return 1;
	}

	return 0;
}

void soundengine_init()
{
	mixer_init();



	// start the audio codec
//#ifndef DEBUG
	CodecInit(SYNTH_FS);
//#endif

	parameterArray_init();

	euklid_init();

	int i;
	for(i=0;i<6;i++)
	{
		modNode_init(&velocityModulators[i]);
	}

	//init the seq speed
	seq_init();
	seq_setBpm(120);

	memset(midi_MidiChannels,0,8);
//	memset(midi_NoteOverride,0,7);

	initDrumVoice();
	Snare_init(&snareVoice, 3);
	HiHat_init(&hatVoice, 5);
	Cymbal_init(&cymbalVoice, 4);

	//precalc the first dma buffer block
	soundengine_calcNextSampleBlock();

	FPU->FPDSCR |= FPU_FPDSCR_FZ_Msk ; //enable flush to zero
}

void soundengine_process()
{
#if CALC_SAMPLES_IN_INTERRUPT
	__HAL_TIM_DISABLE_IT(&htim2, TIM_IT_UPDATE);
	//HAL_TIM_Base_Stop_IT(&htim2);
#if CPU_METER
	cpuMeter_busyEnter();
#endif
	seq_tick();
	soundengine_calcNextSampleBlock();
#if CPU_METER
	cpuMeter_busyExit();
#endif
	__HAL_TIM_SetCounter(&htim2, 1950);
	//HAL_TIM_Base_Start_IT(&htim2);
	__HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
#endif
	//generate next sample if no valid sample is present
#if !CALC_SAMPLES_IN_INTERRUPT
#if 1//#ifndef DEBUG
		seq_tick();
		soundengine_calcNextSampleBlock();



#endif
#endif
		//process midi on midi port
		uart_processMidi();

		//process the sequencer
	//	seq_tick(); //moved to tim 14?
	usb_tick();
	//process incoming frontpanel data
		//	uart_processFront();






	preset_morph_tick();
	//preset_morph_tick();
	//preset_morph_tick();

}
