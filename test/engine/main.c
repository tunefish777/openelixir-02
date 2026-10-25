/*
 * Host render harness for the OpenElixir-02 audio engine.
 *
 * Drives the engine only through entry points that stay stable while the
 * engine is refactored:
 *   soundengine_init()            engine start-up
 *   frontParser_parseUartData()   UI -> engine bytes: parameters, LFO and velocity targets
 *   voiceControl_noteOn()         triggers (tracks 1-7)
 *   mixer_calcNextSampleBlock()   rendering
 * and writes every rendered sample of both DACs to a file, so the output of two
 * source trees can be compared with cmp. Usage: render <output file>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <math.h>
#include "stm32f7xx_hal.h"
#include "main.h"
#include "Soundengine/Soundengine.h"
#include "Soundengine/DSPAudio/mixer.h"
#include "Soundengine/MIDI/MidiVoiceControl.h"
#include "Soundengine/MIDI/CortexParser.h"
#include "Soundengine/MIDI/MidiMessages.h"
#include "Parameters.h"
#include "menu/menu.h"
#include "Preset/InitKit.h"
#include "Soundengine/MIDI/ParameterArray.h"
#include "Soundengine/DSPAudio/wavetable.h"
#include "Soundengine/DSPAudio/transientTables.h"

#define NUM_KITS		32
#define BLOCKS_PER_KIT	400

//---------------------------------------------------------------- environment
static uint32_t rngState = 1;
static uint32_t rnd(uint32_t n)	//deterministic, independent of the engine's RNG
{
	rngState = rngState * 1103515245u + 12345u;
	return (rngState >> 8) % n;
}

static uint32_t hwRngState = 0x12345678;
uint32_t HAL_RNG_GetRandomNumber(RNG_HandleTypeDef *h)
{
	(void)h;
	hwRngState ^= hwRngState << 13;
	hwRngState ^= hwRngState >> 17;
	hwRngState ^= hwRngState << 5;
	return hwRngState;
}

static float faders[6];
float adc_getFaderVolume(uint8_t idx)
{
	return faders[idx];
}

//map the peripheral and core register ranges as plain memory (GPIO jack
//switches, DWT, FPU, timers are read/written by the engine)
static void mapPeripherals(void)
{
	const struct { uintptr_t base; size_t size; } regions[] = {
		{ 0x40000000u, 0x20000000u },
		{ 0xE0000000u, 0x00100000u },
	};
	for(unsigned i = 0; i < sizeof(regions) / sizeof(regions[0]); i++)
	{
		void* p = mmap((void*)regions[i].base, regions[i].size, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
		if(p != (void*)regions[i].base)
		{
			perror("mmap peripheral region");
			exit(2);
		}
	}
}

static void setJackSwitches(uint32_t mask)
{
	*(volatile uint32_t*)&switch_l1_GPIO_Port->IDR = ((mask & 1) ? switch_l1_Pin : 0) | ((mask & 2) ? switch_r1_Pin : 0);
	*(volatile uint32_t*)&switch_l2_GPIO_Port->IDR = ((mask & 4) ? switch_l2_Pin : 0) | ((mask & 8) ? switch_r2_Pin : 0);
}

//---------------------------------------------------------------- UI messages
static void sendFront(uint8_t status, uint8_t data1, uint8_t data2)
{
	frontParser_parseUartData(status);
	frontParser_parseUartData(data1);
	frontParser_parseUartData(data2);
}

static void sendParam(uint16_t par, uint8_t value)
{
	//the UI keeps the kit in activeParameters; the engine reads it back when it
	//resets modulation targets to the unmodulated values
	activeParameters[par] = value;
	if(par < 128)
	{
		sendFront(MIDI_CC, (uint8_t)par, value);
	}
	else
	{
		sendFront(FRONT_CC_2, (uint8_t)(par - 128), value);
	}
}

static void sendTarget(uint8_t status, uint8_t voice, uint8_t param)
{
	sendFront(status, (uint8_t)(((param & 0x80) >> 7) | (voice << 1)), param & 0x7f);
}

//parameters that are not plain sound parameters on the engine side
static int skipParam(uint16_t par)
{
	return par == PAR_NONE || par == PAR_NRPN_DATA_ENTRY_COARSE || par == PAR_NRPN_FINE
		|| par == PAR_NRPN_COARSE || par == PAR_BANK_CHANGE
		|| (par >= PAR_VEL_DEST_1 && par <= PAR_VEL_DEST_6)
		|| (par >= PAR_TARGET_LFO1 && par <= PAR_TARGET_LFO6)
		|| (par >= PAR_VOICE_LFO1 && par <= PAR_VOICE_LFO6);
}

//a random value in the range the UI allows for this parameter (ranges_gen.c)
void harness_valueRange(uint16_t par, uint8_t* min, uint8_t* max);
static uint8_t randomValue(uint16_t par)
{
	uint8_t min, max;
	harness_valueRange(par, &min, &max);
	return (uint8_t)(min + rnd(max - min + 1u));
}

//a random modulation target the UI could send (it only offers parameters the
//engine can modulate, i.e. with a parameterArray pointer)
static uint8_t randomTarget(void)
{
	uint16_t par;
	do { par = (uint16_t)(1 + rnd(END_OF_SOUND_PARAMETERS - 1)); } while(parameterArray[par].ptr == NULL);
	return (uint8_t)par;
}

static void loadKit(int kit)
{
	for(uint16_t par = 0; par < END_OF_SOUND_PARAMETERS; par++)
	{
		if(skipParam(par)) continue;
		const uint8_t init = par < INIT_KIT_SIZE - 8 ? initKit[8 + par] : 0;	//the init kit lacks the last parameter
		sendParam(par, kit == 0 ? init : randomValue(par));
	}
	for(uint8_t v = 0; v < 6; v++)
	{
		sendTarget(FRONT_CC_LFO_TARGET, v, randomTarget());
		sendTarget(FRONT_CC_VELO_TARGET, v, randomTarget());
	}
	for(uint8_t i = 0; i < 6; i++)
	{
		const float f = (float)rnd(128) / 127.f;
		faders[i] = kit == 0 ? 1.f : f * f;
	}
	setJackSwitches(kit == 0 ? 0xf : rnd(16));
}

//---------------------------------------------------------------- integrity
//the constant tables in DTCM are writable memory on the target: an out-of-bounds
//write from a neighbouring buffer would silently change them (and the sound)
static uint32_t fnv(const void* p, size_t n)
{
	const uint8_t* b = p;
	uint32_t h = 2166136261u;
	while(n--) h = (h ^ *b++) * 16777619u;
	return h;
}
static uint32_t tablesHash(void)
{
	return fnv(transientData, sizeof(transientData)) ^ fnv(transientVolumeTable, sizeof(transientVolumeTable)) * 3u
		^ fnv(sine_table, sizeof(sine_table)) * 7u;
}

//---------------------------------------------------------------- engine switching
#if __has_include("Soundengine/DSPAudio/VoiceSlot.h")
#include "Soundengine/DSPAudio/VoiceSlot.h"
#define HAVE_VOICE_SLOTS 1
#if __has_include("Preset/KitEngines.h")
#include "Preset/KitEngines.h"
#include "Soundengine/MIDI/MidiParser.h"
#ifdef KIT_ENGINES_BLOCK_SIZE	//the engine checks need the current KitEngines API (older trees only render)
#define HAVE_KIT_ENGINES 1
#endif

//the UI sends to the sound engine like this in the firmware too
void frontPanel_sendData(uint8_t status, uint8_t data1, uint8_t data2)
{
	sendFront(status, data1, data2);
}
#endif

static int16_t engOut[OUTPUT_DMA_SIZE * 2], engOut2[OUTPUT_DMA_SIZE * 2];

static int slotHasNan(void)
{
	for(int s = 0; s < NUM_VOICE_SLOTS; s++)
	{
		const VoiceCommon* c = &voiceSlots[s].u.c;
		if(isnan(c->egValueOscVol) || isnan(c->filter.s1) || isnan(c->filter.s2) || isnan(c->combinedVolume)) return 1;
	}
	return 0;
}

//render until silent; returns the number of blocks that had sound, peak in *peak
static int renderSound(int maxBlocks, int* peak, int* nan)
{
	int last = -1;
	*peak = 0;
	for(int b = 0; b < maxBlocks; b++)
	{
		mixer_calcNextSampleBlock(engOut, engOut2);
		for(int i = 0; i < OUTPUT_DMA_SIZE * 2; i++)
		{
			const int a = abs(engOut[i]) > abs(engOut2[i]) ? abs(engOut[i]) : abs(engOut2[i]);
			if(a > *peak) *peak = a;
			if(a > 32) last = b;
		}
		*nan |= slotHasNan();
	}
	return last + 1;
}

static void switchEngine(uint8_t slot, uint8_t engine)
{
#ifdef HAVE_KIT_ENGINES
	//like a received NRPN 98: the sound engine switches, then the UI applies the switch rules
	MidiMsg m = { 0 };
	m.status = MIDI_CC2;
	m.data1 = PAR_SLOT_ENGINE - 128;
	m.data2 = (uint8_t)(slot * 4 + engine);
	midiParser_ccHandler(m, 1, 0, 0.f);
	kitEngines_assign(slot, engine);
#else
	slot_setEngine(slot, engine);
#endif
}

#ifdef HAVE_KIT_ENGINES
//Voice level checks render a copy of a slot's voice directly (no mixer, FX or
//other voices), so every run starts from the same state (oscillator phases,
//noise sequence) and only the voice itself is measured.
static void voiceTrigger(VoiceSlot* c, uint8_t slot, uint8_t second)
{
	switch(c->engine)
	{
	case ENGINE_SNARE:	Snare_trigger(&c->u.snare, 127, 60);			break;
	case ENGINE_CYMBAL:	Cymbal_trigger(&c->u.cymbal, 127, 60);			break;
	case ENGINE_HIHAT:	HiHat_trigger(&c->u.hat, 127, second, 60);		break;
	default:			Drum_trigger(&c->u.drum, 127, 60);				break;
	}
	if(slot == NUM_VOICE_SLOTS - 1) c->u.c.oscVolEg.decay = c->decay[second];	//as slot_trigger()
}

//render blocks; returns a hash of the output, *len = number of blocks with sound
static uint32_t voiceRender(VoiceSlot* c, int blocks, int* len)
{
	int16_t buf[OUTPUT_DMA_SIZE];
	uint32_t h = 2166136261u;
	int last = -1;
	for(int b = 0; b < blocks; b++)
	{
		SVF_recalcFreq(&c->u.c.filter);
		switch(c->engine)
		{
		case ENGINE_SNARE:	Snare_calcAsync(&c->u.snare);	Snare_calcSyncBlock(&c->u.snare, buf, OUTPUT_DMA_SIZE);		break;
		case ENGINE_CYMBAL:	Cymbal_calcAsync(&c->u.cymbal);	Cymbal_calcSyncBlock(&c->u.cymbal, buf, OUTPUT_DMA_SIZE);	break;
		case ENGINE_HIHAT:	HiHat_calcAsync(&c->u.hat);		HiHat_calcSyncBlock(&c->u.hat, buf, OUTPUT_DMA_SIZE);		break;
		default:			calcDrumVoiceAsync(&c->u.drum);	calcDrumVoiceSyncBlock(&c->u.drum, buf, OUTPUT_DMA_SIZE);	break;
		}
		h = fnv(buf, sizeof(buf)) ^ h * 16777619u;
		for(int i = 0; i < OUTPUT_DMA_SIZE; i++) if(abs(buf[i]) > 32) last = b;
	}
	if(len) *len = last + 1;
	return h;
}

static uint32_t slotSoundHash(uint8_t slot)
{
	static VoiceSlot c;
	c = voiceSlots[slot];
	hwRngState = 0x12345678;
	voiceTrigger(&c, slot, 0);
	return voiceRender(&c, 200, NULL);
}

//slot 6: track 7 must use decay B (longer here) and cut a sounding track 7 the
//other way round: track 6 after track 7 ends no later than track 6 alone
static int decayTest(void)
{
	static VoiceSlot c;
	int lenA, lenB, lenCut;
	const uint8_t slot = NUM_VOICE_SLOTS - 1;
	c = voiceSlots[slot]; hwRngState = 0x12345678;
	voiceTrigger(&c, slot, 0); voiceRender(&c, 3000, &lenA);
	c = voiceSlots[slot]; hwRngState = 0x12345678;
	voiceTrigger(&c, slot, 1); voiceRender(&c, 3000, &lenB);
	c = voiceSlots[slot]; hwRngState = 0x12345678;
	voiceTrigger(&c, slot, 1); voiceRender(&c, 5, NULL);
	voiceTrigger(&c, slot, 0); voiceRender(&c, 3000, &lenCut);
	printf("  slot 6 decay A %d, B %d, B cut by A %d", lenA, lenB, lenCut);
	return !(lenB > lenA && lenCut <= lenA);
}

//every engine specific parameter of a slot must change the sound of the engine
//the slot runs, except the roles that engine doesn't have
static int roleTest(int round)
{
	int fail = 0;
	printf("  roles (x = changes the sound):");
	for(uint8_t s = 0; s < NUM_VOICE_SLOTS; s++)
	{
		const uint8_t engine = voiceSlots[s].engine;
		printf("  %d:", s + 1);
		//an active setting first, so that turning one role to 0 must change the
		//sound: roles 0-5 at 100 (FM amount > 0, waveform PWM, ...), role 6 at 1
		//(mix mode on, 1 repeat; many repeats can silence a short attack), and
		//the slot's PWM at 64 (PWM at pulse width 0 is silent) and its main
		//oscillator a sine (a noise carrier ignores the FM modulators)
		static const uint8_t oscWave[NUM_VOICE_SLOTS] = { PAR_OSC_WAVE_DRUM1, PAR_OSC_WAVE_DRUM2, PAR_OSC_WAVE_DRUM3, PAR_OSC_WAVE_SNARE, PAR_WAVE1_CYM, PAR_WAVE1_HH };
		uint8_t saved[NUM_SLOT_ROLES];
		const uint8_t savedPwm = activeParameters[PAR_PWM_1 + s];
		const uint8_t savedWave = activeParameters[oscWave[s]];
		sendParam(PAR_PWM_1 + s, 64);
		sendParam(oscWave[s], SINE);
		for(uint8_t role = 0; role < NUM_SLOT_ROLES; role++)
		{
			saved[role] = activeParameters[slot_rolePar[s][role]];
			sendParam(slot_rolePar[s][role], role == 6 ? 1 : 100);
		}
		for(uint8_t role = 0; role < NUM_SLOT_ROLES; role++)
		{
			const uint8_t par = slot_rolePar[s][role];
			const uint32_t a = slotSoundHash(s);
			sendParam(par, 0);
			const uint32_t b = slotSoundHash(s);
			sendParam(par, role == 6 ? 1 : 100);
			const int unused = (engine == ENGINE_SNARE && role == 0) || (engine == ENGINE_HIHAT && role == 6);
			printf("%c", a != b ? 'x' : '.');
			if((a != b) == unused)
			{
				fail = 1;
			}
		}
		for(uint8_t role = 0; role < NUM_SLOT_ROLES; role++)
		{
			sendParam(slot_rolePar[s][role], saved[role]);
		}
		sendParam(PAR_PWM_1 + s, savedPwm);
		sendParam(oscWave[s], savedWave);
	}
	printf("\n");
	(void)round;
	return fail;
}
#endif

#ifdef HAVE_KIT_ENGINES
//the engine block in kit files: round trip, and stock engines for anything else
static int blockTest(void)
{
	static const uint8_t engines[NUM_VOICE_SLOTS] = { ENGINE_HIHAT, ENGINE_SNARE, ENGINE_DRUM, ENGINE_CYMBAL, ENGINE_DRUM, ENGINE_SNARE };
	uint8_t stock[NUM_VOICE_SLOTS], out[NUM_VOICE_SLOTS], block[32];
	int fail = 0;
	kitEngines_setStock(stock);
	const uint8_t size = kitEngines_encode(engines, block);
	kitEngines_decode(block, size, out);
	fail |= memcmp(out, engines, sizeof(out)) != 0;
	kitEngines_decode(block, (uint16_t)(size + 10), out);		//more bytes after the block
	fail |= memcmp(out, engines, sizeof(out)) != 0;
	kitEngines_decode(block, (uint16_t)(size - 1), out);		//truncated
	fail |= memcmp(out, stock, sizeof(out)) != 0;
	kitEngines_decode(block, 0, out);							//no block (stock firmware kit)
	fail |= memcmp(out, stock, sizeof(out)) != 0;
	block[1] = 'X';												//not a block
	kitEngines_decode(block, size, out);
	fail |= memcmp(out, stock, sizeof(out)) != 0;
	block[1] = 'E'; block[5] = 9;								//invalid engine: that slot stock
	kitEngines_decode(block, size, out);
	fail |= out[0] != stock[0] || memcmp(&out[1], &engines[1], NUM_VOICE_SLOTS - 1) != 0;
	printf("engine block in kit files: %s\n", fail ? "FAILED" : "ok");
	return fail;
}
#endif

//every slot gets every engine: each track must sound, track 7 must use decay B,
//track 6 must cut track 7, and the engine specific parameters must reach the engine
static int engineTest(void)
{
	static const char* const names[NUM_ENGINES] = { "drum", "snare", "cymbal", "hihat" };
	int fail = 0, peak, nan = 0;
#ifdef HAVE_KIT_ENGINES
	fail |= blockTest();
#endif
	for(int round = 0; round < NUM_ENGINES; round++)
	{
#ifdef HAVE_KIT_ENGINES
		kitEngines_setStock(kitEngines);	//like loading a stock kit
		kitEngines_apply();
#endif
		loadKit(0);
		for(uint8_t v = 0; v < 6; v++)
		{
			sendTarget(FRONT_CC_LFO_TARGET, v, 0);
			sendTarget(FRONT_CC_VELO_TARGET, v, 0);
		}
		for(uint8_t s = 0; s < NUM_VOICE_SLOTS; s++)
		{
			switchEngine(s, (s + round) % NUM_ENGINES);
		}
		sendParam(PAR_VELOD6_CLOSED, 40);
		sendParam(PAR_VELOD6_OPEN, 90);
		renderSound(400, &peak, &nan);

		printf("round %d:", round);
		int len[7];
		for(uint8_t track = 0; track < 7; track++)
		{
			voiceControl_noteOn(track, 60, 127);
			len[track] = renderSound(1500, &peak, &nan);
			printf("  %d %s %d/%d", track + 1, names[voiceSlots[track < 6 ? track : 5].engine], len[track], peak);
			if(len[track] == 0) fail = 1;
		}
		printf("\n");
#ifdef HAVE_KIT_ENGINES
		fail |= decayTest();
		fail |= roleTest(round);
#endif
	}
	if(nan) printf("NaN in a voice\n");
	printf(fail || nan ? "ENGINE TEST FAILED\n" : "engine test passed\n");
	return fail || nan;
}
#endif

//---------------------------------------------------------------- main
int main(int argc, char** argv)
{
#ifdef HAVE_VOICE_SLOTS
	if(argc == 2 && strcmp(argv[1], "--engines") == 0)
	{
		mapPeripherals();
		for(int i = 0; i < 6; i++) faders[i] = 1.f;
		setJackSwitches(0xf);
		soundengine_init();
		return engineTest();
	}
#endif
	if(argc != 2)
	{
		fprintf(stderr, "usage: %s <output file> | --engines\n", argv[0]);
		return 2;
	}
	mapPeripherals();
	FILE* out = fopen(argv[1], "wb");
	if(!out)
	{
		perror(argv[1]);
		return 2;
	}

	for(int i = 0; i < 6; i++) faders[i] = 1.f;
	setJackSwitches(0xf);
	soundengine_init();

	int16_t output[OUTPUT_DMA_SIZE * 2];
	int16_t output2[OUTPUT_DMA_SIZE * 2];

	const uint32_t tables = tablesHash();
	for(int kit = 0; kit < NUM_KITS; kit++)
	{
		loadKit(kit);
		uint32_t hash = 2166136261u;
		for(int block = 0; block < BLOCKS_PER_KIT; block++)
		{
			//triggers: dense, every track, random notes and velocities
			for(uint8_t track = 0; track < 7; track++)
			{
				if(rnd(8) == 0)
				{
					voiceControl_noteOn(track, (uint8_t)(24 + rnd(72)), (uint8_t)(1 + rnd(127)));
				}
			}
			//parameter changes while voices sound (knob turns)
			if(kit > 0 && rnd(4) == 0)
			{
				uint16_t par;
				do { par = (uint16_t)rnd(END_OF_SOUND_PARAMETERS); } while(skipParam(par));
				sendParam(par, randomValue(par));
			}

			mixer_calcNextSampleBlock(output, output2);
			fwrite(output, sizeof(output), 1, out);
			fwrite(output2, sizeof(output2), 1, out);
			for(unsigned i = 0; i < sizeof(output) / 2; i++)
			{
				hash = (hash ^ (uint16_t)output[i]) * 16777619u;
				hash = (hash ^ (uint16_t)output2[i]) * 16777619u;
			}
		}
		printf("kit %2d %08x\n", kit, hash);
		if(tablesHash() != tables)
		{
			fprintf(stderr, "kit %d: constant tables in DTCM were overwritten (out-of-bounds write)\n", kit);
			return 3;
		}
	}
	fclose(out);
	return 0;
}
