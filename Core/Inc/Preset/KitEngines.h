/*
 * KitEngines.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * The voice engine of each slot in the active kit (UI side). Changing an engine
 * follows the same rules wherever it comes from (MIDI now, the menu later):
 * all sound parameters of the slot (incl. volume, pan, output, LFO) start from
 * the new engine's defaults, modulation targets on its engine specific
 * parameters are turned off, and the slot's parameters are sent to the sound
 * engine again.
 */

#ifndef KITENGINES_H_
#define KITENGINES_H_

#include "stm32f7xx_hal.h"
#include "Soundengine/DSPAudio/VoiceSlot.h"

//Kit files (.snd, also in projects and the tmp folder) and tmp.snd carry the
//engines in a block appended after the 247 parameter bytes:
//	"OE02", payload size, payload: the engine of each slot (6 bytes),
//	the Perf group LFO target of each LFO (6 bytes, 0 = none, n = n-th entry)
//Later versions may only append to the payload. Files without the block (from
//the stock firmware or anywhere else) load with the stock engines, and the
//stock firmware ignores the block. LFOs with a Perf target are stored in the
//parameter bytes as LFO on its own voice, off (the stock firmware doesn't know
//the group), the block restores them.
#define KIT_ENGINES_BLOCK_SIZE	(5 + NUM_VOICE_SLOTS)
#define KIT_BLOCK_SIZE			(5 + 2*NUM_VOICE_SLOTS)

//version of this interface (for the host tests, which also build older trees)
#define KIT_ENGINES_API		3

extern uint8_t kitEngines[NUM_VOICE_SLOTS];							//the active kit
extern uint8_t kitEngines_ram[NUM_KIT_SLOTS][NUM_VOICE_SLOTS];		//the kits in RAM (mainParameters)

//all kits: stock engines
void kitEngines_init(void);
void kitEngines_setStock(uint8_t* engines);

//change the engine of a slot in the active kit
void kitEngines_assign(const uint8_t slot, const uint8_t engine);

//load a voice from another kit (its parameters kit[], END_OF_SOUND_PARAMETERS)
//into the same slot of the active kit, with its engine
void kitEngines_loadVoice(const uint8_t slot, const uint8_t engine, const uint8_t* kit);

//set the sound engine's slots to the active kit's engines (before the kit's
//parameters are sent)
void kitEngines_apply(void);

//the slot whose engine specific parameter par is, NUM_VOICE_SLOTS if none
uint8_t kitEngines_rolePar2Slot(const uint16_t par);

//the slot whose sound parameter par is (incl. its modulation targets and slot 6
//decay B), NUM_VOICE_SLOTS if none
uint8_t kitEngines_slotOfParam(const uint16_t par);

//the parameter of slot toSlot with the same function as par of slot fromSlot
//(slot 6 decay B maps to decay); par itself if it isn't a sound parameter of fromSlot
uint16_t kitEngines_mapParam(uint16_t par, const uint8_t fromSlot, const uint8_t toSlot);

//Engine descriptions (UI side). A stock engine takes its menu page, value types, defaults and
//modulation target names from its stock slot. An engine without a stock slot takes its page
//layout and the defaults of its common parameters from a template slot and describes its 7
//engine specific parameters (roles) itself; template parameters it has no use for are hidden.
#define ENGINE_NO_STOCK_SLOT	0xFF
typedef struct
{
	const char* name;						//engine menu ("drum")
	const char* shortName;					//3 characters (voice load list)
	uint8_t stockSlot;						//slot whose stock engine this is, or ENGINE_NO_STOCK_SLOT
	//engines without a stock slot:
	uint8_t templateSlot;					//page layout and defaults of the common parameters
	uint8_t roleText[NUM_SLOT_ROLES];		//names (TEXT_*); TEXT_EMPTY: role unused (hidden)
	uint16_t roleDtype[NUM_SLOT_ROLES];		//value types (DTYPE_*)
	uint8_t roleInit[NUM_SLOT_ROLES];		//defaults
	uint8_t numHidden;
	const uint8_t* hidden;					//template slot parameters without function
} KitEngineInfo;
extern const KitEngineInfo kitEngines_info[NUM_ENGINES];

//the slot an engine's menu page, value types and (common) defaults come from
uint8_t kitEngines_pageSlot(const uint8_t engine);
//par is a role parameter of a slot whose engine has no stock slot: its role (and *engine),
//otherwise NUM_SLOT_ROLES
uint8_t kitEngines_customRole(const uint16_t par, uint8_t* engine);
//par is a parameter of a slot whose engine has no stock slot and has no function there
uint8_t kitEngines_isHiddenParam(const uint16_t par);
//a parameter of an engine's template page (engines without stock slot): PAR_NONE if hidden;
//for a role parameter *text becomes its name
uint16_t kitEngines_templatePar(const uint8_t engine, const uint16_t par, uint8_t* text);

//engines -> block, returns its size (KIT_ENGINES_BLOCK_SIZE)
uint8_t kitEngines_encode(const uint8_t* engines, uint8_t* block);
//block (size bytes read after the parameters, maybe 0) -> engines; the stock
//engines if it isn't a valid block
void kitEngines_decode(const uint8_t* block, const uint16_t size, uint8_t* engines);

//the whole block (engines and Perf LFO targets of the kit params[]), returns its size (KIT_BLOCK_SIZE)
uint8_t kitEngines_encodeKit(const uint8_t* engines, const uint8_t* params, uint8_t* block);
//block -> engines and the Perf LFO targets into params[] (params may be NULL)
void kitEngines_decodeKit(const uint8_t* block, const uint16_t size, uint8_t* engines, uint8_t* params);
//the kit params[] as the stock firmware gets them in the file: LFOs with a Perf target
//on their own voice, off
void kitEngines_stockLfoTargets(uint8_t* params);

#endif /* KITENGINES_H_ */
