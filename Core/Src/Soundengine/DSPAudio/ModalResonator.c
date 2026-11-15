/*
 * ModalResonator.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */
#include "Soundengine/DSPAudio/ModalResonator.h"
#include <math.h>
#include <string.h>

#define MODAL_PI			3.14159265f
#ifdef MODAL_MEASURE
float modal_peakPreLimit;		//host prototype: level before the soft limit
#endif
#define MODAL_NYQUIST_LIMIT	(0.45f * MODAL_SAMPLE_RATE)
#define MODAL_HOLD_R		0.9999922f		//hold: -60 dB after 20 s

//Frequency ratios of the modes for 4 materials; structure blends between neighbours
//(in the log domain, mode by mode). All rows rise, so do the blends: the first mode
//above the Nyquist limit ends the list.
static const float modal_ratios[4][MODAL_MAX_MODES] =
{
	//string / tube: harmonic
	{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
	//free bar (xylophone, marimba without tuning): ~((2n+1)/3)^2
	{ 1.000f, 2.778f, 5.444f, 9.000f, 13.44f, 18.78f, 25.00f, 32.11f, 40.11f, 49.00f, 58.78f, 69.44f,
	  81.00f, 93.44f, 106.8f, 121.0f, 136.1f, 152.1f, 169.0f, 186.8f, 205.4f, 225.0f, 245.4f, 266.8f },
	//circular membrane (drum skin): zeros of the Bessel functions relative to the first
	{ 1.000f, 1.593f, 2.135f, 2.295f, 2.653f, 2.917f, 3.155f, 3.500f, 3.598f, 3.647f, 4.058f, 4.153f,
	  4.230f, 4.600f, 4.832f, 4.903f, 5.131f, 5.412f, 5.517f, 5.650f, 5.932f, 6.000f, 6.209f, 6.305f },
	//bell / plate: bell partials (hum, prime, tierce, quint, nominal, ...) relative to the hum
	{ 1.000f, 2.000f, 2.380f, 3.000f, 4.000f, 5.030f, 5.330f, 6.000f, 6.700f, 8.000f, 8.700f, 9.600f,
	  10.40f, 11.20f, 12.10f, 13.00f, 13.90f, 14.80f, 15.90f, 16.90f, 18.00f, 19.10f, 20.20f, 21.40f },
};

//------------------------------------------------------------------------------------
void modal_init(ModalResonator* m)
{
	memset(m, 0, sizeof(*m));
	m->f0 = 220.f;
	m->structure = 0.f;
	m->brightness = 0.5f;
	m->damping = 0.5f;
	m->position = 0.3f;
	m->hardness = 0.5f;
	m->exciter = MODAL_EXC_MALLET;
	m->partials = 16;
	m->noise = 0x12345678;
	m->a2h = -MODAL_HOLD_R * MODAL_HOLD_R;
	m->dirty = 1;
}
//------------------------------------------------------------------------------------
static float modal_clamp01(float v)
{
	return v < 0.f ? 0.f : v > 1.f ? 1.f : v;
}
void modal_setFrequency(ModalResonator* m, float f0)	{ if(f0 != m->f0) { m->f0 = f0; m->dirty = 1; } }
void modal_setStructure(ModalResonator* m, float v)		{ m->structure = modal_clamp01(v); m->dirty = 1; }
void modal_setBrightness(ModalResonator* m, float v)	{ m->brightness = modal_clamp01(v); m->dirty = 1; }
void modal_setDamping(ModalResonator* m, float v)		{ m->damping = modal_clamp01(v); m->dirty = 1; }
void modal_setPosition(ModalResonator* m, float v)		{ m->position = modal_clamp01(v); m->dirty = 1; }
void modal_setHardness(ModalResonator* m, float v)		{ m->hardness = modal_clamp01(v); m->dirty = 1; }
void modal_setExciter(ModalResonator* m, uint8_t e)		{ m->exciter = e < MODAL_NUM_EXCITERS ? e : MODAL_NUM_EXCITERS-1; m->dirty = 1; }
void modal_setPartials(ModalResonator* m, uint8_t p)
{
	m->partials = p > MODAL_MAX_MODES ? MODAL_MAX_MODES : p < 1 ? 1 : p;
	m->dirty = 1;
}
//------------------------------------------------------------------------------------
//ratio of mode k for the current structure
static float modal_ratio(const ModalResonator* m, uint8_t k)
{
	const float s = m->structure * 3.f;
	uint8_t row = (uint8_t)s;
	if(row > 2)
		row = 2;
	const float t = s - row;
	const float a = modal_ratios[row][k];
	const float b = modal_ratios[row+1][k];
	return a * expf(t * logf(b / a));
}
//------------------------------------------------------------------------------------
//mode coefficients from the settings (once per block if something changed)
static void modal_update(ModalResonator* m)
{
	//ringing of the first mode: 30 ms .. 9 s
	const float t0 = 0.03f * powf(300.f, m->damping);
	//dark: high modes die faster and are quieter
	const float dark = 1.f - m->brightness;
	const float hfDamping = 2.f * dark * dark;
	const float tilt = 1.5f * dark;
	//strike position: 0 = at the edge .. 1 = in the middle
	const float p = 0.02f + 0.48f * m->position;
	const uint8_t noiseExcited = m->exciter != MODAL_EXC_MALLET;

	float sum = 0.f;
	float sumSq = 0.f;
	uint8_t n = 0;
	for(uint8_t k=0;k<m->partials;k++)
	{
		const float ratio = modal_ratio(m, k);
		const float f = m->f0 * ratio;
		if(f >= MODAL_NYQUIST_LIMIT)
			break;
		const float w = 2.f * MODAL_PI * f / MODAL_SAMPLE_RATE;
		const float t = t0 / (1.f + hfDamping * (ratio - 1.f));
		const float r = expf(-6.9078f / (t * MODAL_SAMPLE_RATE));		//-60 dB after t
		float amp = 0.1f + 0.9f * fabsf(sinf(MODAL_PI * (k + 1) * p));
		amp *= powf((float)(k + 1), -tilt);

		m->a1[k] = 2.f * r * cosf(w);
		m->a2[k] = -r * r;
		m->a1h[k] = 2.f * MODAL_HOLD_R * cosf(w);
		//an impulse rings with gain/sin(w); noise builds up with the resonance, so it
		//gets less input the longer the mode rings
		m->gain[k] = amp * sinf(w) * (noiseExcited ? sqrtf(1.f - r * r) : 1.f);
		sum += amp;
		sumSq += amp * amp;
		n++;
	}
	//modes that just went above the limit stop where they are
	for(uint8_t k=n;k<m->numModes;k++)
	{
		m->y1[k] = m->y2[k] = 0.f;
	}
	m->numModes = n;
	//a mallet starts all modes in phase: their peaks add up; noise adds them up like a sum of
	//random signals (the levels below came from listening tests of the host prototype)
	if(sum <= 0.f)
		m->outGain = 0.f;
	else if(noiseExcited)
		m->outGain = (m->exciter == MODAL_EXC_SUSTAIN ? 0.45f : 4.f) / sqrtf(sumSq);
	else
		m->outGain = 0.9f / sum;

	//exciter: harder = shorter (mallet 6 ms .. 2 samples, noise 80 ms .. 1 ms) and brighter
	const float soft = 1.f - m->hardness;
	if(m->exciter == MODAL_EXC_MALLET)
		m->exLen = 2 + (uint32_t)(soft * soft * 0.006f * MODAL_SAMPLE_RATE);
	else
		m->exLen = (uint32_t)((0.001f + soft * soft * 0.08f) * MODAL_SAMPLE_RATE);
	//a noise burst keeps its loudness when it gets longer
	m->exBurstGain = m->exciter == MODAL_EXC_NOISE ? sqrtf(0.002f * MODAL_SAMPLE_RATE / m->exLen) : 1.f;
	if(m->exBurstGain > 1.f)
		m->exBurstGain = 1.f;
	const float cutoff = 300.f * powf(60.f, m->hardness);		//300 Hz .. 18 kHz
	m->exLpCoef = 1.f - expf(-2.f * MODAL_PI * cutoff / MODAL_SAMPLE_RATE);

	m->dirty = 0;
}
//------------------------------------------------------------------------------------
void modal_trigger(ModalResonator* m, float velocity)
{
	if(m->dirty)
		modal_update(m);
	//the previous strike still rings: fade it out instead of cutting it (a cut clicks), the new
	//one starts from rest like every other engine's oscillators
	if(m->active)
	{
		memcpy(m->fy1, m->y1, sizeof(m->fy1));
		memcpy(m->fy2, m->y2, sizeof(m->fy2));
		m->fadeModes = m->numModes;
		m->fadeLeft = MODAL_FADE_SAMPLES;
	}
	memset(m->y1, 0, sizeof(m->y1));
	memset(m->y2, 0, sizeof(m->y2));
	//the noise exciters start the same way every time
	m->noise = 0x12345678;
	m->exLp = 0.f;

	m->exPos = 0;
	m->exAmp = modal_clamp01(velocity);
	m->held = 1;
	m->active = 1;
	//mallet: sin(pi*n/len) for n = 0..len-1
	const float step = MODAL_PI / m->exLen;
	m->exC = 2.f * cosf(step);
	m->exS2 = -sinf(step);	//"s[-1]"
	m->exS1 = 0.f;
}
//------------------------------------------------------------------------------------
void modal_release(ModalResonator* m)
{
	m->held = 0;
}
//------------------------------------------------------------------------------------
void modal_setHold(ModalResonator* m, uint8_t hold)
{
	m->hold = hold;
}
//------------------------------------------------------------------------------------
static float modal_noise(ModalResonator* m)
{
	m->noise = m->noise * 1664525u + 1013904223u;
	return (int32_t)m->noise * (1.f / 2147483648.f);
}
//------------------------------------------------------------------------------------
//a mode state that went NaN/infinite (would stay silent) restarts from 0; the exponent bits
//are checked, -ffast-math removes x != x tests
static float modal_finite(float v)
{
	union { float f; uint32_t u; } b = { v };
	return (b.u & 0x7F800000) == 0x7F800000 ? 0.f : v;
}
//------------------------------------------------------------------------------------
uint8_t modal_calcBlock(ModalResonator* m, float* out, float* fadeOut, uint16_t size)
{
	if(m->dirty)
		modal_update(m);
	if(!m->active && !(m->exciter == MODAL_EXC_SUSTAIN && m->held))
	{
		memset(out, 0, size * sizeof(float));
		return 0;
	}

	//the exciter of this block (0 once the impulse/burst is over)
	float ex[size];
	uint8_t excited = 0;
	for(uint16_t i=0;i<size;i++)
	{
		float x = 0.f;
		switch(m->exciter)
		{
		case MODAL_EXC_MALLET:
			//half-sine impulse; its area stays the same, so a soft mallet only loses the highs
			if(m->exPos < m->exLen)
			{
				x = m->exAmp * (MODAL_PI / m->exLen) * m->exS1;
				const float s = m->exC * m->exS1 - m->exS2;
				m->exS2 = m->exS1;
				m->exS1 = s;
			}
			break;
		case MODAL_EXC_NOISE:
			if(m->exPos < m->exLen)
			{
				m->exLp += m->exLpCoef * (modal_noise(m) - m->exLp);
				x = m->exAmp * m->exBurstGain * m->exLp * (1.f - (float)m->exPos / m->exLen);
			}
			break;
		default:	//MODAL_EXC_SUSTAIN
			if(m->held)
			{
				m->exLp += m->exLpCoef * (modal_noise(m) - m->exLp);
				x = m->exAmp * m->exLp;
			}
			break;
		}
		if(m->exPos < 0xFFFFFFFF)
			m->exPos++;
		ex[i] = x;
		excited |= x != 0.f;
	}

	memset(out, 0, size * sizeof(float));
	const uint8_t hold = m->hold && m->exciter != MODAL_EXC_SUSTAIN;
	for(uint8_t k=0;k<m->numModes;k++)
	{
		const float a1 = hold ? m->a1h[k] : m->a1[k];
		const float a2 = hold ? m->a2h : m->a2[k];
		const float g = m->gain[k];
		float y1 = m->y1[k];
		float y2 = m->y2[k];
		if(excited)
		{
			for(uint16_t i=0;i<size;i++)
			{
				const float y = a1 * y1 + a2 * y2 + g * ex[i];
				y2 = y1;
				y1 = y;
				out[i] += y;
			}
		}
		else
		{
			for(uint16_t i=0;i<size;i++)
			{
				const float y = a1 * y1 + a2 * y2;
				y2 = y1;
				y1 = y;
				out[i] += y;
			}
		}
		m->y1[k] = modal_finite(y1);
		m->y2[k] = modal_finite(y2);
	}

	//the previous strike, fading out linearly (into out or its own buffer)
	uint8_t faded = 0;
	if(m->fadeLeft)
	{
		float* dst = out;
		if(fadeOut)
		{
			memset(fadeOut, 0, size * sizeof(float));
			dst = fadeOut;
			faded = 1;
		}
		float env[size];
		for(uint16_t i=0;i<size;i++)
		{
			env[i] = i < m->fadeLeft ? (float)(m->fadeLeft - i) / MODAL_FADE_SAMPLES : 0.f;
		}
		for(uint8_t k=0;k<m->fadeModes && k<m->numModes;k++)
		{
			const float a1 = m->a1[k];
			const float a2 = m->a2[k];
			float y1 = m->fy1[k];
			float y2 = m->fy2[k];
			for(uint16_t i=0;i<size;i++)
			{
				const float y = a1 * y1 + a2 * y2;
				y2 = y1;
				y1 = y;
				dst[i] += y * env[i];
			}
			m->fy1[k] = modal_finite(y1);
			m->fy2[k] = modal_finite(y2);
		}
		m->fadeLeft = m->fadeLeft > size ? (uint16_t)(m->fadeLeft - size) : 0;
	}

	//level and a soft limit to [-1:1]; died away: not computed until the next trigger
	float peak = 0.f;
	for(uint16_t i=0;i<size;i++)
	{
		peak = fmaxf(peak, fabsf(out[i]));
		float x = out[i] * m->outGain;
#ifdef MODAL_MEASURE
		if(fabsf(x) > modal_peakPreLimit) modal_peakPreLimit = fabsf(x);
#endif
		x = x < -1.5f ? -1.5f : x > 1.5f ? 1.5f : x;
		out[i] = x - (4.f / 27.f) * x * x * x;
	}
	if(faded)
	{
		for(uint16_t i=0;i<size;i++)
		{
			float x = fadeOut[i] * m->outGain;
			x = x < -1.5f ? -1.5f : x > 1.5f ? 1.5f : x;
			fadeOut[i] = x - (4.f / 27.f) * x * x * x;
		}
	}
	if(!excited && !m->fadeLeft && peak * m->outGain < 0.00003f)
	{
		m->active = 0;
		memset(m->y1, 0, sizeof(m->y1));
		memset(m->y2, 0, sizeof(m->y2));
	}
	return faded;
}
