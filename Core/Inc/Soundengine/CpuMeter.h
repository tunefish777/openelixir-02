/*
 * CpuMeter.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * CPU load measurement for development builds (cmake -DLXR_CPU_METER=ON).
 * Counts the cycles of the audio work per 64-sample block with the DWT cycle
 * counter:
 *  - dsp:   mixer_calcNextSampleBlock() (all voices + FX)
 *  - audio: everything in the audio interrupt and soundengine_process()
 *           (sequencer tick, block rendering, MIDI), i.e. what the main loop
 *           (UI) cannot use.
 *  - per section: the render function of each voice, the FX bus, and the rest
 *    of the mixer (LFOs, envelopes, decimation, panning/summing, limiter).
 * Values are shown on two pages; SHIFT + PROJECT steps through them (menu.c).
 * cpuMeter.stressFm renders the cymbal engine that many extra times per block,
 * to simulate kits with more FM voices (SHIFT + MORPH cycles 0..4).
 */

#ifndef CPU_METER_H_
#define CPU_METER_H_

#include "stm32f7xx_hal.h"
#include "Soundengine/config.h"

#if CPU_METER

#define CPU_METER_MAX_STRESS 4

//the first blocks after boot or a reset include start-up work, don't count them as peaks
#define CPU_METER_SETTLE_BLOCKS 16

enum
{
	CPU_SECTION_VOICE1,	//voices 1-6 (CPU_SECTION_VOICE1 + voice index)
	CPU_SECTION_FX = 6,
	CPU_SECTION_OTHER,	//rest of mixer_calcNextSampleBlock()
	CPU_NUM_SECTIONS
};

typedef struct
{
	uint32_t busyStart;		/**< cycle counter when the current audio work started */
	uint32_t busyAcc;		/**< audio work cycles since the start of the current block */
	uint32_t dspLast;		/**< cycles of the last rendered block */
	uint32_t dspPeak;
	uint32_t busyLast;		/**< audio work cycles of the last complete block period */
	uint32_t busyPeak;
	uint32_t blocks;		/**< rendered blocks since the last reset */
	int underrunBase;		/**< DMA underrun count at the last reset */
	uint32_t sectionAcc;	/**< cycles of the measured sections in the current block */
	uint32_t sectionPeak[CPU_NUM_SECTIONS];
	uint8_t stressFm;		/**< extra cymbal engine renders per block */
} CpuMeter;

extern CpuMeter cpuMeter;

/** call at the start / end of the audio interrupt and of soundengine_process() */
static inline void cpuMeter_busyEnter(void)
{
	cpuMeter.busyStart = DWT->CYCCNT;
}
static inline void cpuMeter_busyExit(void)
{
	cpuMeter.busyAcc += DWT->CYCCNT - cpuMeter.busyStart;
}

/** call after a measured section of mixer_calcNextSampleBlock() with the
 * cycle counter value from its start */
static inline void cpuMeter_section(uint8_t section, uint32_t start)
{
	const uint32_t cycles = DWT->CYCCNT - start;
	cpuMeter.sectionAcc += cycles;
	if(cpuMeter.blocks > CPU_METER_SETTLE_BLOCKS && cycles > cpuMeter.sectionPeak[section])
	{
		cpuMeter.sectionPeak[section] = cycles;
	}
}

/** call around mixer_calcNextSampleBlock() */
uint32_t cpuMeter_dspStart(void);
void cpuMeter_dspEnd(uint32_t start);

void cpuMeter_resetPeaks(void);
/** audio buffer underruns (block not ready in time) since the last reset */
uint32_t cpuMeter_underruns(void);
/** cycles available per audio block (OUTPUT_DMA_SIZE samples) */
uint32_t cpuMeter_blockCycles(void);

#endif /* CPU_METER */
#endif /* CPU_METER_H_ */
