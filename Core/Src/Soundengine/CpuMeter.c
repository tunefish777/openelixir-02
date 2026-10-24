/*
 * CpuMeter.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "Soundengine/CpuMeter.h"

#if CPU_METER

CpuMeter cpuMeter;

//underrun counter of the I2S DMA callback (cs4344_cs5343.c)
extern volatile int error;

uint32_t cpuMeter_dspStart(void)
{
	const uint32_t now = DWT->CYCCNT;

	//a new block starts: close the audio work account of the previous block period
	const uint32_t busy = cpuMeter.busyAcc + (now - cpuMeter.busyStart);
	cpuMeter.busyAcc = 0;
	cpuMeter.busyStart = now;
	cpuMeter.busyLast = busy;
	if(cpuMeter.blocks > CPU_METER_SETTLE_BLOCKS && busy > cpuMeter.busyPeak)
	{
		cpuMeter.busyPeak = busy;
	}
	return now;
}

void cpuMeter_dspEnd(uint32_t start)
{
	const uint32_t cycles = DWT->CYCCNT - start;
	cpuMeter.dspLast = cycles;
	if(cpuMeter.blocks > CPU_METER_SETTLE_BLOCKS)
	{
		if(cycles > cpuMeter.dspPeak)
		{
			cpuMeter.dspPeak = cycles;
		}
		//everything in the mixer that is not a voice or the FX bus
		const uint32_t other = cycles - cpuMeter.sectionAcc;
		if(other > cpuMeter.sectionPeak[CPU_SECTION_OTHER])
		{
			cpuMeter.sectionPeak[CPU_SECTION_OTHER] = other;
		}
	}
	cpuMeter.sectionAcc = 0;
	cpuMeter.blocks++;
}

void cpuMeter_resetPeaks(void)
{
	cpuMeter.dspPeak = 0;
	cpuMeter.busyPeak = 0;
	cpuMeter.blocks = 0;
	cpuMeter.underrunBase = error;
	for(uint8_t i=0;i<CPU_NUM_SECTIONS;i++)
	{
		cpuMeter.sectionPeak[i] = 0;
	}
}

uint32_t cpuMeter_underruns(void)
{
	return (uint32_t)(error - cpuMeter.underrunBase);
}

uint32_t cpuMeter_blockCycles(void)
{
	return (uint32_t)(OUTPUT_DMA_SIZE * (SystemCoreClock / REAL_FS));
}

#endif
