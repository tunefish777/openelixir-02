/*
 * ModalResonator.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * Struck resonator for the "Modal" voice engine: an exciter (mallet, noise
 * burst or a sustained noise "bow") drives up to 24 tuned two-pole resonators
 * (modes). Their frequency ratios (structure: harmonic -> bar -> membrane ->
 * bell), decay (damping, brightness) and level (strike position, brightness)
 * decide the material. Own design; the idea follows modal synthesis as in
 * Mutable Instruments Plaits (no code taken from it).
 */
#ifndef MODALRESONATOR_H_
#define MODALRESONATOR_H_

#include <stdint.h>

#define MODAL_MAX_MODES		24
#define MODAL_SAMPLE_RATE	44140.f		//REAL_FS of the firmware

enum ModalExciter
{
	MODAL_EXC_MALLET,	//a short impulse, harder = shorter and brighter
	MODAL_EXC_NOISE,	//a filtered noise burst, hardness = shorter and brighter
	MODAL_EXC_SUSTAIN,	//continuous filtered noise while the voice is held ("bow")
	MODAL_NUM_EXCITERS
};
#define MODAL_NUM_PARTIAL_SETTINGS	4	//partials 8, 12, 16, 24
#define MODAL_FADE_SAMPLES	88		//2 ms: a new strike fades out the ringing of the previous one

typedef struct
{
	//modes: y[n] = a1*y[n-1] + a2*y[n-2] + gain*x[n]
	float y1[MODAL_MAX_MODES];
	float y2[MODAL_MAX_MODES];
	//the ringing of the previous strike, faded out over MODAL_FADE_SAMPLES after a new one
	float fy1[MODAL_MAX_MODES];
	float fy2[MODAL_MAX_MODES];
	uint16_t fadeLeft;
	uint8_t fadeModes;
	float a1[MODAL_MAX_MODES];
	float a2[MODAL_MAX_MODES];
	float gain[MODAL_MAX_MODES];
	//hold: the modes ring (almost) without decay, e.g. during a long amp envelope attack
	float a1h[MODAL_MAX_MODES];
	float a2h;
	uint8_t hold;
	uint8_t numModes;		//modes computed (partials, minus those above the Nyquist limit)

	//settings [0:1], see modal_set*
	float f0;				//pitch of the first mode in Hz
	float structure;
	float brightness;
	float damping;
	float position;
	float hardness;
	uint8_t exciter;		//enum ModalExciter
	uint8_t partials;		//8, 12, 16 or 24
	uint8_t dirty;			//coefficients have to be recalculated

	//exciter
	uint32_t exPos;			//samples since the trigger
	uint32_t exLen;			//length of the impulse / noise burst in samples
	float exAmp;			//velocity
	float exLp;				//one-pole lowpass of the noise
	float exLpCoef;
	float exBurstGain;		//noise burst: longer = less loud per sample
	float exS1, exS2, exC;	//mallet: half sine by recurrence (s[n+1] = c*s[n] - s[n-1])
	uint32_t noise;			//noise generator state
	uint8_t held;			//sustained exciter: on until modal_release
	uint8_t active;			//rings or is excited (a silent resonator isn't computed)
	float outGain;			//normalizes the sum of the modes
} ModalResonator;

void modal_init(ModalResonator* m);
//pitch in Hz; the settings are [0:1]
void modal_setFrequency(ModalResonator* m, float f0);
void modal_setStructure(ModalResonator* m, float v);
void modal_setBrightness(ModalResonator* m, float v);
void modal_setDamping(ModalResonator* m, float v);
void modal_setPosition(ModalResonator* m, float v);
void modal_setHardness(ModalResonator* m, float v);
void modal_setExciter(ModalResonator* m, uint8_t exciter);
void modal_setPartials(ModalResonator* m, uint8_t partials);
//strike with velocity [0:1] (the sustained exciter starts and runs until modal_release). Every
//strike starts from rest (identical hits); a still ringing resonator is faded out meanwhile
void modal_trigger(ModalResonator* m, float velocity);
void modal_release(ModalResonator* m);
//hold the ringing (no decay) while on; not for the sustained exciter, which keeps exciting anyway
void modal_setHold(ModalResonator* m, uint8_t hold);
//renders size samples [-1:1] (soft limited). fadeOut: the fading ringing of the previous
//strike separately (returns 1 if it wrote it), or NULL to mix it into out
uint8_t modal_calcBlock(ModalResonator* m, float* out, float* fadeOut, uint16_t size);

#endif /* MODALRESONATOR_H_ */
