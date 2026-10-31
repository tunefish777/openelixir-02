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
	cpuMeter.lastRender = now;
	//time this block waited since it was requested
	const uint32_t late = now - cpuMeter.requestTime;
	if(cpuMeter.blocks > CPU_METER_SETTLE_BLOCKS && late > cpuMeter.latePeak)
	{
		cpuMeter.latePeak = late;
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
	cpuMeter.dmaLast[0] = cpuMeter.dmaLast[1] = 0;
	cpuMeter.dmaMin = UINT32_MAX;
	cpuMeter.dmaMax = 0;
	cpuMeter.latePeak = 0;
	cpuMeter.seqLatePeakUs = 0;
	cpuMeter.stallPeak = 0;
	cpuMeter.stallPc = 0;
	cpuMeter.stallBlocked = 0;
}

void cpuMeter_irqSample(const uint32_t* frame)
{
	const uint32_t now = DWT->CYCCNT;
	const uint32_t block = cpuMeter_blockCycles();
	//TIM14 runs every 0.25 ms: a much longer interval means it couldn't run
	const uint8_t blocked = (now - cpuMeter.lastSample) > block;
	cpuMeter.lastSample = now;

	const uint32_t gap = now - cpuMeter.lastRender;
	if(cpuMeter.blocks > CPU_METER_SETTLE_BLOCKS && gap > 2 * block && gap > cpuMeter.stallPeak)
	{
		cpuMeter.stallPeak = gap;
		cpuMeter.stallPc = frame[6];	//stacked PC: r0-r3, r12, lr, pc, xpsr
		cpuMeter.stallBlocked = blocked;
	}
}

void cpuMeter_seqTickLate(float lateTicks)
{
	//systick_ticks run at 4 kHz
	const uint32_t us = lateTicks > 0 ? (uint32_t)(lateTicks * 250.f) : 0;
	if(us > cpuMeter.seqLatePeakUs)
	{
		cpuMeter.seqLatePeakUs = us;
	}
}

uint32_t cpuMeter_adcLoad(void)
{
	const uint32_t now = DWT->CYCCNT;
	const uint32_t window = now - cpuMeter.adcWindowStart;
	const uint32_t acc = cpuMeter.adcAcc;
	cpuMeter.adcAcc = 0;
	cpuMeter.adcWindowStart = now;
	return window ? (uint32_t)((uint64_t)acc * 100 / window) : 0;
}

uint32_t cpuMeter_dmaGapUs(void)
{
	//the shortest interval is the block period: the rest is how late a callback was
	if(cpuMeter.dmaMax < cpuMeter.dmaMin)
		return 0;
	return (cpuMeter.dmaMax - cpuMeter.dmaMin) / (SystemCoreClock / 1000000);
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
