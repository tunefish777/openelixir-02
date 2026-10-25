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
//	"OE02", payload size, payload: the engine of each slot (6 bytes)
//Later versions may only append to the payload. Files without the block (from
//the stock firmware or anywhere else) load with the stock engines, and the
//stock firmware ignores the block.
#define KIT_ENGINES_BLOCK_SIZE	(5 + NUM_VOICE_SLOTS)

//version of this interface (for the host tests, which also build older trees)
#define KIT_ENGINES_API		2

extern uint8_t kitEngines[NUM_VOICE_SLOTS];							//the active kit
extern uint8_t kitEngines_ram[NUM_KIT_SLOTS][NUM_VOICE_SLOTS];		//the kits in RAM (mainParameters)

//all kits: stock engines
void kitEngines_init(void);
void kitEngines_setStock(uint8_t* engines);

//change the engine of a slot in the active kit
void kitEngines_assign(const uint8_t slot, const uint8_t engine);

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

//the slot whose stock engine engine is (where its defaults and menu pages come from)
uint8_t kitEngines_stockSlot(const uint8_t engine);

//engines -> block, returns its size (KIT_ENGINES_BLOCK_SIZE)
uint8_t kitEngines_encode(const uint8_t* engines, uint8_t* block);
//block (size bytes read after the parameters, maybe 0) -> engines; the stock
//engines if it isn't a valid block
void kitEngines_decode(const uint8_t* block, const uint16_t size, uint8_t* engines);

#endif /* KITENGINES_H_ */
