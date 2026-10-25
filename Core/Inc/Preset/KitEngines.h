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

extern uint8_t kitEngines[NUM_VOICE_SLOTS];

//change the engine of a slot in the active kit
void kitEngines_assign(const uint8_t slot, const uint8_t engine);

//all slots back to their stock engines, without changing the kit's parameter
//values (before a kit is loaded: kits don't store engines yet)
void kitEngines_restoreStock(void);

#endif /* KITENGINES_H_ */
