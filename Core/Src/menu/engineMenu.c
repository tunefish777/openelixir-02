/*
 * engineMenu.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "menu/engineMenu.h"
#include "menu/menu.h"
#include "Preset/KitEngines.h"
#include "buttonHandler.h"
#include "ledHandler.h"
#include <stdio.h>
static uint8_t engineMenu_slot;
static uint8_t engineMenu_choice;

static const char* const engineMenu_names[NUM_ENGINES] = { "drum", "snare", "cymbal", "hi-hat" };
//---------------------------------------------------
void engineMenu_open(void)
{
	//voice 7 is the 2nd trigger of slot 6
	const uint8_t voice = menu_getActiveVoice();
	engineMenu_slot = voice < NUM_VOICE_SLOTS ? voice : NUM_VOICE_SLOTS-1;
	engineMenu_choice = kitEngines[engineMenu_slot];

	//like the load/save menu: own select mode (step buttons do nothing, no steps shown)
	buttonHandler_setMode(SELECT_MODE_ENGINE);
	led_clearAllBlinkLeds();
	led_clearSequencerLeds();
	menu_switchPage(ENGINE_PAGE);
	led_setBlinkLed(LED_STEP8, 1);	//the KIT button blinks while the menu is open
}
//---------------------------------------------------
void engineMenu_apply(void)
{
	if(!engineMenu_shown)
		return;
	kitEngines_assign(engineMenu_slot, engineMenu_choice);
	menu_repaintAll();
}

//---------------------------------------------------
void engineMenu_encoder(const int8_t inc)
{
	int8_t choice = (int8_t)engineMenu_choice + inc;
	if(choice < 0) choice = 0;
	if(choice >= NUM_ENGINES) choice = NUM_ENGINES-1;
	engineMenu_choice = (uint8_t)choice;
	menu_repaintAll();
}
//---------------------------------------------------
void engineMenu_repaint(char lines[2][17])
{
	//voice 6 and 7 share the slot
	if(engineMenu_slot == NUM_VOICE_SLOTS-1)
		snprintf(lines[0], 17, "voice 6+7 engine");
	else
		snprintf(lines[0], 17, "voice %u engine", engineMenu_slot+1);
	//"?" while the choice differs from the current engine
	snprintf(lines[1], 17, "[%s]%s", engineMenu_names[engineMenu_choice],
			engineMenu_choice == kitEngines[engineMenu_slot] ? "" : "?");
}
