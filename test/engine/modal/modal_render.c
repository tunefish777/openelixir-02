/*
 * modal_render.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * Host prototype of the Modal engine's resonator: renders parameter sweeps as
 * WAV files for listening (test/engine/modal.sh). Each sweep is a row of hits,
 * one per setting, 1.2 s apart.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "Soundengine/DSPAudio/ModalResonator.h"
extern float modal_peakPreLimit;

#define FS			44140
#define BLOCK		64
#define HIT_SAMPLES	(FS * 12 / 10)
#define MAX_HITS	12

static int16_t wav[MAX_HITS * HIT_SAMPLES];
static float peakAll;
static long modeBlocks, blocks;

static void writeWav(const char* dir, const char* name, int samples)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/%s.wav", dir, name);
	FILE* f = fopen(path, "wb");
	if(!f) { perror(path); exit(2); }
	const unsigned data = (unsigned)samples * 2, rate = FS, byteRate = FS * 2;
	const unsigned riff = 36 + data, fmtLen = 16;
	const unsigned short pcm = 1, ch = 1, align = 2, bits = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
	fwrite(&fmtLen, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
	fwrite(&rate, 4, 1, f); fwrite(&byteRate, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f); fwrite(wav, 2, (size_t)samples, f);
	fclose(f);
}

//one hit of the resonator with the current settings into the wav buffer at hit index
static void renderHit(ModalResonator* m, int hit, float velocity, int holdSamples, float* peak)
{
	float buf[BLOCK];
	modal_trigger(m, velocity);
	for(int pos = 0; pos < HIT_SAMPLES; pos += BLOCK)
	{
		if(pos >= holdSamples)
			modal_release(m);
		modal_calcBlock(m, buf, NULL, BLOCK);
		modeBlocks += m->numModes;
		blocks++;
		for(int i = 0; i < BLOCK && pos + i < HIT_SAMPLES; i++)
		{
			const float v = buf[i];
			if(isnan(v)) { fprintf(stderr, "NaN in output\n"); exit(3); }
			if(fabsf(v) > *peak) *peak = fabsf(v);
			wav[hit * HIT_SAMPLES + pos + i] = (int16_t)lrintf(v * 32767.f);
		}
	}
	//silence before the next hit (the next one starts from rest)
	ModalResonator next = *m;
	memset(next.y1, 0, sizeof(next.y1));
	memset(next.y2, 0, sizeof(next.y2));
	*m = next;
}

typedef void (*Setter)(ModalResonator*, float);

//a sweep of one setting over n hits, the others at their defaults
static void sweep(const char* dir, const char* name, Setter set, int n, float f0, uint8_t exciter)
{
	ModalResonator m;
	modal_init(&m);
	modal_setFrequency(&m, f0);
	modal_setExciter(&m, exciter);
	printf("%-12s", name);
	for(int h = 0; h < n; h++)
	{
		float peak = 0.f;
		const float v = n > 1 ? (float)h / (n - 1) : 0.5f;
		if(set) set(&m, v);
		modal_peakPreLimit = 0.f;
		renderHit(&m, h, 1.f, exciter == MODAL_EXC_SUSTAIN ? FS * 8 / 10 : 0, &peak);
		printf(" %4.2f", modal_peakPreLimit);
		if(peak > peakAll) peakAll = peak;
	}
	printf("   (level before the limit)\n");
	writeWav(dir, name, n * HIT_SAMPLES);
}

int main(int argc, char** argv)
{
	const char* dir = argc > 1 ? argv[1] : ".";
	printf("sizeof(ModalResonator) = %u bytes\n", (unsigned)sizeof(ModalResonator));

	sweep(dir, "structure", modal_setStructure, 9, 220.f, MODAL_EXC_MALLET);
	sweep(dir, "damping", modal_setDamping, 7, 220.f, MODAL_EXC_MALLET);
	sweep(dir, "brightness", modal_setBrightness, 7, 220.f, MODAL_EXC_MALLET);
	sweep(dir, "position", modal_setPosition, 7, 220.f, MODAL_EXC_MALLET);
	sweep(dir, "hardness", modal_setHardness, 7, 220.f, MODAL_EXC_MALLET);
	sweep(dir, "noise_hard", modal_setHardness, 5, 220.f, MODAL_EXC_NOISE);
	sweep(dir, "sustain", modal_setStructure, 5, 220.f, MODAL_EXC_SUSTAIN);

	//partials 8/12/16/24 on a bell
	{
		ModalResonator m;
		modal_init(&m);
		modal_setStructure(&m, 1.f);
		modal_setDamping(&m, 0.75f);
		const uint8_t partials[4] = { 8, 12, 16, 24 };
		printf("%-12s", "partials");
		for(int h = 0; h < 4; h++)
		{
			float peak = 0.f;
			modal_setPartials(&m, partials[h]);
			renderHit(&m, h, 1.f, 0, &peak);
			printf(" %4.2f", peak);
		}
		printf("   (8/12/16/24 on a bell)\n");
		writeWav(dir, "partials", 4 * HIT_SAMPLES);
	}

	//a pitch run over the whole range: modes above the Nyquist limit drop out
	{
		ModalResonator m;
		modal_init(&m);
		modal_setStructure(&m, 0.4f);
		modal_setPartials(&m, 24);
		printf("%-12s", "pitch");
		for(int h = 0; h < 10; h++)
		{
			float peak = 0.f;
			const float f0 = 27.5f * powf(2.f, h * 0.8f);
			modal_setFrequency(&m, f0);
			renderHit(&m, h, 1.f, 0, &peak);
			printf(" %2u", m.numModes);
		}
		printf("   (modes computed, 27 Hz .. 7 kHz)\n");
		writeWav(dir, "pitch", 10 * HIT_SAMPLES);
	}

	printf("highest peak %.2f, average %.1f modes per block\n", peakAll, (double)modeBlocks / blocks);
	return 0;
}
