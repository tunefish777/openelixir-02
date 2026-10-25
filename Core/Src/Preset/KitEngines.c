/*
 * KitEngines.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "Preset/KitEngines.h"
#include "Preset/InitKit.h"
#include "menu/menu.h"
#include "Soundengine/MIDI/MidiMessages.h"
#include "frontPanelParser_ui.h"
#include "Preset/PresetManager.h"
#include "Soundengine/Sequencer/sequencer.h"
#include <string.h>

uint8_t kitEngines[NUM_VOICE_SLOTS] = { ENGINE_DRUM, ENGINE_DRUM, ENGINE_DRUM, ENGINE_SNARE, ENGINE_CYMBAL, ENGINE_HIHAT };
uint8_t kitEngines_ram[NUM_KIT_SLOTS][NUM_VOICE_SLOTS];

static const char kitEngines_signature[4] = { 'O', 'E', '0', '2' };
//---------------------------------------------------
void kitEngines_setStock(uint8_t* engines)
{
	for(uint8_t slot=0;slot<NUM_VOICE_SLOTS;slot++)
	{
		engines[slot] = slot_stockEngine(slot);
	}
}
//---------------------------------------------------
void kitEngines_init(void)
{
	kitEngines_setStock(kitEngines);
	for(uint8_t kit=0;kit<NUM_KIT_SLOTS;kit++)
	{
		kitEngines_setStock(kitEngines_ram[kit]);
	}
}
//---------------------------------------------------
uint8_t kitEngines_encode(const uint8_t* engines, uint8_t* block)
{
	memcpy(block, kitEngines_signature, 4);
	block[4] = NUM_VOICE_SLOTS;
	memcpy(&block[5], engines, NUM_VOICE_SLOTS);
	return KIT_ENGINES_BLOCK_SIZE;
}
//---------------------------------------------------
void kitEngines_decode(const uint8_t* block, const uint16_t size, uint8_t* engines)
{
	kitEngines_setStock(engines);
	if(size < KIT_ENGINES_BLOCK_SIZE || memcmp(block, kitEngines_signature, 4) != 0 || block[4] < NUM_VOICE_SLOTS)
		return;
	for(uint8_t slot=0;slot<NUM_VOICE_SLOTS;slot++)
	{
		if(block[5+slot] < NUM_ENGINES)
			engines[slot] = block[5+slot];
	}
}
//---------------------------------------------------
uint8_t kitEngines_stockSlot(const uint8_t engine)
{
	switch(engine)
	{
	case ENGINE_SNARE:	return 3;
	case ENGINE_CYMBAL:	return 4;
	case ENGINE_HIHAT:	return 5;
	default:			return 0;
	}
}
//---------------------------------------------------
static uint8_t kitEngines_isRolePar(const uint8_t slot, const uint16_t par)
{
	for(uint8_t role=0;role<NUM_SLOT_ROLES;role++)
	{
		if(slot_rolePar[slot][role] == par)
			return 1;
	}
	return 0;
}
//---------------------------------------------------
uint8_t kitEngines_rolePar2Slot(const uint16_t par)
{
	for(uint8_t slot=0;slot<NUM_VOICE_SLOTS;slot++)
	{
		if(kitEngines_isRolePar(slot, par))
			return slot;
	}
	return NUM_VOICE_SLOTS;
}
//---------------------------------------------------
static void kitEngines_sendPar(const uint8_t par)
{
	if(par < 128)
		frontPanel_sendData(MIDI_CC, par, activeParameters[par]);
	else
		frontPanel_sendData(CC_2, (uint8_t)(par-128), activeParameters[par]);
}
//---------------------------------------------------
#define NUM_PER_SLOT_GROUPS	21
#define NUM_SLOT_PARAMS		(6 + NUM_PER_SLOT_GROUPS + NUM_SLOT_ROLES)
//The sound parameters of a slot, in the same order for every slot (entry i has
//the same function in all slots). Slot 6 additionally has decay B.
static void kitEngines_slotParams(const uint8_t slot, uint8_t* list)
{
	static const uint8_t oscWave[NUM_VOICE_SLOTS] = { PAR_OSC_WAVE_DRUM1, PAR_OSC_WAVE_DRUM2, PAR_OSC_WAVE_DRUM3, PAR_OSC_WAVE_SNARE, PAR_WAVE1_CYM, PAR_WAVE1_HH };
	static const uint8_t pan[NUM_VOICE_SLOTS] = { PAR_PAN1, PAR_PAN2, PAR_PAN3, PAR_PAN4, PAR_PAN5, PAR_PAN6 };
	//parameter groups with one parameter per slot in slot order
	static const uint8_t perSlot[NUM_PER_SLOT_GROUPS] =
	{
		PAR_FILTER_FREQ_1, PAR_RESO_1, PAR_VOL_SLOPE1, PAR_VOL1, PAR_DRIVE1, PAR_FILTER_DRIVE_1,
		PAR_FILTER_TYPE_1, PAR_VOLUME_MOD_ON_OFF1, PAR_TRANS1_VOL, PAR_TRANS1_WAVE, PAR_TRANS1_FREQ, PAR_PWM_1,
		PAR_VOICE_DECIMATION1, PAR_AUDIO_OUT1, PAR_VELO_MOD_AMT_1,
		PAR_FREQ_LFO1, PAR_AMOUNT_LFO1, PAR_WAVE_LFO1, PAR_RETRIGGER_LFO1, PAR_SYNC_LFO1, PAR_OFFSET_LFO1,
	};
	uint8_t n = 0;
	list[n++] = oscWave[slot];
	list[n++] = PAR_COARSE1 + 2*slot;
	list[n++] = PAR_FINE1 + 2*slot;
	list[n++] = PAR_VELOA1 + 2*slot;
	list[n++] = PAR_VELOD1 + 2*slot;
	list[n++] = pan[slot];
	for(uint8_t i=0;i<NUM_PER_SLOT_GROUPS;i++)
	{
		list[n++] = perSlot[i] + slot;
	}
	for(uint8_t role=0;role<NUM_SLOT_ROLES;role++)
	{
		list[n++] = slot_rolePar[slot][role];
	}
}
//---------------------------------------------------
//parameters of a slot that are not in kitEngines_slotParams(), with one entry per slot
static const uint8_t kitEngines_extraParams[] = { PAR_VEL_DEST_1, PAR_VOICE_LFO1, PAR_TARGET_LFO1 };
//---------------------------------------------------
uint8_t kitEngines_slotOfParam(const uint16_t par)
{
	if(par == PAR_VELOD6_OPEN)
		return NUM_VOICE_SLOTS-1;
	uint8_t list[NUM_SLOT_PARAMS];
	for(uint8_t slot=0;slot<NUM_VOICE_SLOTS;slot++)
	{
		for(uint8_t i=0;i<sizeof(kitEngines_extraParams);i++)
		{
			if(par == kitEngines_extraParams[i] + slot)
				return slot;
		}
		kitEngines_slotParams(slot, list);
		for(uint8_t i=0;i<NUM_SLOT_PARAMS;i++)
		{
			if(list[i] == par)
				return slot;
		}
	}
	return NUM_VOICE_SLOTS;
}
//---------------------------------------------------
uint16_t kitEngines_mapParam(uint16_t par, const uint8_t fromSlot, const uint8_t toSlot)
{
	if(fromSlot == toSlot)
		return par;
	if(par == PAR_VELOD6_OPEN && fromSlot == NUM_VOICE_SLOTS-1)
		par = PAR_VELOD6_CLOSED;
	for(uint8_t i=0;i<sizeof(kitEngines_extraParams);i++)
	{
		if(par == kitEngines_extraParams[i] + fromSlot)
			return kitEngines_extraParams[i] + toSlot;
	}
	uint8_t from[NUM_SLOT_PARAMS], to[NUM_SLOT_PARAMS];
	kitEngines_slotParams(fromSlot, from);
	kitEngines_slotParams(toSlot, to);
	for(uint8_t i=0;i<NUM_SLOT_PARAMS;i++)
	{
		if(from[i] == par)
			return to[i];
	}
	return par;
}
//---------------------------------------------------
static uint8_t kitEngines_initValue(const uint8_t par)
{
	return par < INIT_KIT_SIZE - 8 ? initKit[8 + par] : 0;
}
//---------------------------------------------------
static void kitEngines_turnOffTarget(const uint8_t par, const uint8_t status, const uint8_t voice)
{
	activeParameters[par] = 0;
	frontPanel_sendData(status, (uint8_t)(voice<<1), 0);
}
//---------------------------------------------------
void kitEngines_assign(const uint8_t slot, const uint8_t engine)
{
	if(slot >= NUM_VOICE_SLOTS || engine >= NUM_ENGINES || kitEngines[slot] == engine)
		return;

	kitEngines[slot] = engine;
	slot_setEngine(slot, engine);
	menu_updateVoicePages();

	//every sound parameter of the slot starts from the new engine's defaults:
	//the values of the engine's stock slot in the init kit
	uint8_t list[NUM_SLOT_PARAMS], stockList[NUM_SLOT_PARAMS];
	const uint8_t stockSlot = kitEngines_stockSlot(engine);
	kitEngines_slotParams(slot, list);
	kitEngines_slotParams(stockSlot, stockList);
	for(uint8_t i=0;i<NUM_SLOT_PARAMS;i++)
	{
		activeParameters[list[i]] = kitEngines_initValue(stockList[i]);
	}
	if(slot == NUM_VOICE_SLOTS-1)
	{
		//decay B: the hi-hat's own, otherwise the same as decay A
		activeParameters[PAR_VELOD6_OPEN] = kitEngines_initValue(stockSlot == slot ? PAR_VELOD6_OPEN : PAR_VELOD1 + 2*stockSlot);
	}

	//the slot's own LFO and velocity modulation start off (copied targets would
	//point at the stock slot's voice); the LFO's voice is the slot itself
	activeParameters[PAR_VOICE_LFO1+slot] = slot+1;
	kitEngines_turnOffTarget(PAR_TARGET_LFO1+slot, CC_LFO_TARGET, slot);
	kitEngines_turnOffTarget(PAR_VEL_DEST_1+slot, CC_VELO_TARGET, slot);

	//modulation of the slot's engine specific parameters by other voices would
	//now change a different setting: turn it off
	for(uint8_t i=0;i<NUM_VOICE_SLOTS;i++)
	{
		if(kitEngines_isRolePar(slot, modTargets[activeParameters[PAR_TARGET_LFO1+i]].param))
		{
			kitEngines_turnOffTarget(PAR_TARGET_LFO1+i, CC_LFO_TARGET, i);
		}
		if(kitEngines_isRolePar(slot, modTargets[activeParameters[PAR_VEL_DEST_1+i]].param))
		{
			kitEngines_turnOffTarget(PAR_VEL_DEST_1+i, CC_VELO_TARGET, i);
		}
	}

	//the same for step automation in the patterns in RAM
	for(uint8_t pattern=0;pattern<NUM_PATTERN;pattern++)
	{
		for(uint8_t track=0;track<NUM_TRACKS;track++)
		{
			for(uint8_t step=0;step<NUM_STEPS;step++)
			{
				Step* st = &seq_patternSet.seq_subStepPattern[pattern][track][step];
				if(kitEngines_isRolePar(slot, st->param1Nr))
				{
					st->param1Nr = NO_AUTOMATION;
					st->param1Val = 0;
				}
				if(kitEngines_isRolePar(slot, st->param2Nr))
				{
					st->param2Nr = NO_AUTOMATION;
					st->param2Val = 0;
				}
			}
		}
	}

	//autosave (tmp.snd) like any other kit edit
	preset_touchTimer(PRESET_KIT_TIMER);

	//the voice was re-initialized: send all its parameters
	for(uint8_t i=0;i<NUM_SLOT_PARAMS;i++)
	{
		kitEngines_sendPar(list[i]);
	}
	if(slot == NUM_VOICE_SLOTS-1)
	{
		kitEngines_sendPar(PAR_VELOD6_OPEN);
	}
	menu_repaint();
}
//---------------------------------------------------
void kitEngines_apply(void)
{
	for(uint8_t slot=0;slot<NUM_VOICE_SLOTS;slot++)
	{
		slot_setEngine(slot, kitEngines[slot]);
	}
	menu_updateVoicePages();
}
