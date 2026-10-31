/*
 * DjFilter.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "Soundengine/DSPAudio/DjFilter.h"
#include "Soundengine/config.h"
#include <math.h>

//cutoff range: low-pass from open down to 40 Hz, high-pass from 20 Hz up to 8 kHz
#define DJ_LP_OPEN			20000.f
#define DJ_LP_CLOSED		40.f
#define DJ_HP_OPEN			20.f
#define DJ_HP_CLOSED		8000.f
//resonance: damping from 1.414 (flat, no peak) down to 0.05 (peak of about +26 dB)
#define DJ_DAMPING_MIN		1.414f
#define DJ_DAMPING_MAX		0.05f
//knob smoothing per block (about 10 ms): no zipper noise from the 7-bit knob steps
#define DJ_SMOOTHING		0.15f
//below this smoothed position the filter is fully open and can switch off or change type
#define DJ_POS_OFF			0.002f

volatile uint8_t djFilter_value = DJ_FILTER_OFF;
volatile uint8_t djFilter_reso = 0;

//state: 4 channels (both stereo outputs), zero delay feedback state variable filter
INDTCMZ static float djFilter_ic1[4];
INDTCMZ static float djFilter_ic2[4];
INDTCMZ static float djFilter_pos;		//smoothed knob position 0 (open) .. 1 (closed)
INDTCMZ static int8_t djFilter_mode;	//0 off, -1 low-pass, 1 high-pass
INDTCMZ static float djFilter_coef[4];	//a1, a2, a3, k of the last block (interpolated from there)

typedef struct
{
	float a1, a2, a3, k;	//at the start of the block
	float d1, d2, d3, dk;	//change per sample
	float fade;				//>= 0: crossfade from the filter to the dry signal (switching off)
	int8_t mode;
} DjCoefs;

//one channel of an interleaved stereo buffer; the coefficients move linearly over the block
//(not inlined: one copy instead of four, ITCM is small)
IN_ITCM_RAM __attribute__((noinline)) static void djFilter_channel(int32_t* buf, float* ic1eq, float* ic2eq, const DjCoefs* c)
{
	float ic1 = *ic1eq;
	float ic2 = *ic2eq;
	float a1 = c->a1, a2 = c->a2, a3 = c->a3, k = c->k;
	const float fadeStep = 1.f / OUTPUT_DMA_SIZE;
	float dry = 0.f;
	for(uint32_t i=0;i<OUTPUT_DMA_SIZE*2;i+=2)
	{
		const float v0 = (float)buf[i];
		const float v3 = v0 - ic2;
		const float v1 = a1*ic1 + a2*v3;
		const float v2 = ic2 + a2*ic1 + a3*v3;
		ic1 = 2.f*v1 - ic1;
		ic2 = 2.f*v2 - ic2;
		float out = c->mode < 0 ? v2 : v0 - k*v1 - v2;
		if(c->fade >= 0.f)
		{
			dry += fadeStep;
			out += (v0 - out) * dry;
		}
		buf[i] = (int32_t)out;
		a1 += c->d1; a2 += c->d2; a3 += c->d3; k += c->dk;
	}
	*ic1eq = ic1;
	*ic2eq = ic2;
}

//once per block: runs from flash, only the sample loop is in ITCM
uint8_t djFilter_process(int32_t* mix, int32_t* mix2)
{
	//target from the knob: type and how far it is closed
	const int16_t offset = (int16_t)djFilter_value - DJ_FILTER_OFF;
	const int16_t amount = (offset < 0 ? -offset : offset) - DJ_FILTER_DEAD_ZONE;
	const int8_t targetMode = amount <= 0 ? 0 : (offset < 0 ? -1 : 1);
	float target = amount <= 0 ? 0.f : amount / (float)(DJ_FILTER_OFF - DJ_FILTER_DEAD_ZONE);
	if(target > 1.f) target = 1.f;

	DjCoefs c;
	c.fade = -1.f;
	c.mode = djFilter_mode;

	//a change of type (or off) first opens the filter fully
	if(targetMode != djFilter_mode)
	{
		if(djFilter_mode != 0 && djFilter_pos < DJ_POS_OFF)
		{
			//fully open: this block fades from the filter to the dry signal, then off
			c.fade = 0.f;
			target = 0.f;
		}
		else if(djFilter_mode == 0)
		{
			djFilter_mode = c.mode = targetMode;
			djFilter_pos = 0.f;
			for(uint8_t ch=0;ch<4;ch++)
			{
				//low-pass starts as if it had passed the signal all along, high-pass from silence
				djFilter_ic1[ch] = 0.f;
				djFilter_ic2[ch] = 0.f;
			}
			if(targetMode < 0)
			{
				djFilter_ic2[0] = (float)mix[0];
				djFilter_ic2[1] = (float)mix[1];
				djFilter_ic2[2] = (float)mix2[0];
				djFilter_ic2[3] = (float)mix2[1];
			}
		}
		else
		{
			target = 0.f;
		}
	}
	if(djFilter_mode == 0)
		return 0;

	const uint8_t starting = djFilter_pos == 0.f && c.fade < 0.f;
	djFilter_pos += (target - djFilter_pos) * DJ_SMOOTHING;

	const float fc = djFilter_mode < 0 ?
			DJ_LP_OPEN * powf(DJ_LP_CLOSED / DJ_LP_OPEN, djFilter_pos) :
			DJ_HP_OPEN * powf(DJ_HP_CLOSED / DJ_HP_OPEN, djFilter_pos);
	const float g = tanf(3.14159265f * fc / REAL_FS);
	const float k = DJ_DAMPING_MIN * powf(DJ_DAMPING_MAX / DJ_DAMPING_MIN, djFilter_reso / 127.f);
	const float a1 = 1.f / (1.f + g*(g + k));
	const float a2 = g * a1;
	const float a3 = g * a2;
	if(starting)
	{
		djFilter_coef[0] = a1; djFilter_coef[1] = a2; djFilter_coef[2] = a3; djFilter_coef[3] = k;
	}
	c.a1 = djFilter_coef[0]; c.a2 = djFilter_coef[1]; c.a3 = djFilter_coef[2]; c.k = djFilter_coef[3];
	const float step = 1.f / OUTPUT_DMA_SIZE;
	c.d1 = (a1 - c.a1) * step; c.d2 = (a2 - c.a2) * step; c.d3 = (a3 - c.a3) * step; c.dk = (k - c.k) * step;
	djFilter_coef[0] = a1; djFilter_coef[1] = a2; djFilter_coef[2] = a3; djFilter_coef[3] = k;

	djFilter_channel(&mix[0], &djFilter_ic1[0], &djFilter_ic2[0], &c);
	djFilter_channel(&mix[1], &djFilter_ic1[1], &djFilter_ic2[1], &c);
	djFilter_channel(&mix2[0], &djFilter_ic1[2], &djFilter_ic2[2], &c);
	djFilter_channel(&mix2[1], &djFilter_ic1[3], &djFilter_ic2[3], &c);

	if(c.fade >= 0.f)
	{
		//faded out completely: off from the next block on (bit-identical to no filter)
		djFilter_mode = 0;
		djFilter_pos = 0.f;
	}
	return 1;
}
