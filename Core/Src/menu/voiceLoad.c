/*
 * voiceLoad.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "menu/voiceLoad.h"
#include "menu/menu.h"
#include "menu/projectBrowse.h"
#include "Preset/PresetManager.h"
#include "Preset/KitEngines.h"
#include "buttonHandler.h"
#include "ledHandler.h"
#include <stdio.h>
#include <string.h>

static uint8_t voiceLoad_voice;		//track 0-6 (6 = 2nd trigger of slot 6)
static uint8_t voiceLoad_kit;		//position in the kit list of the listed project

static const char* const voiceLoad_voiceNames[7] = { "Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "CL HH", "OP HH" };
static const char* const voiceLoad_engineNames[NUM_ENGINES] = { "drm", "snr", "cym", "hht" };
//---------------------------------------------------
static uint8_t voiceLoad_slot(void)
{
	return voiceLoad_voice < NUM_VOICE_SLOTS ? voiceLoad_voice : NUM_VOICE_SLOTS-1;
}
//---------------------------------------------------
void voiceLoad_open(const uint8_t voice)
{
	const uint8_t wasOpen = menu_activePage == VOICE_LOAD_PAGE;
	if(wasOpen)
		led_setBlinkLed(led_idx2StepLed(voiceLoad_voice), 0);
	voiceLoad_voice = voice;

	//opened again for another voice: stay in the project and at the kit, so
	//several voices of one kit can be loaded one after the other
	if(!wasOpen)
	{
		projectBrowse_reset();
		voiceLoad_kit = menu_selectedPresetNr[SAVE_TYPE_KIT];

		//like the load kit menu: load mode, step buttons don't show the steps
		buttonHandler_setMode(SELECT_MODE_LOAD);
		led_setMode2(SELECT_MODE_LOAD);
		led_clearAllBlinkLeds();
		menu_switchPage(VOICE_LOAD_PAGE);
		led_clearSequencerLeds();
	}
	led_setBlinkLed(led_idx2StepLed(voiceLoad_voice), 1);
	menu_repaintAll();
}
//---------------------------------------------------
void voiceLoad_leave(void)
{
	led_setBlinkLed(led_idx2StepLed(voiceLoad_voice), 0);
}
//---------------------------------------------------
void voiceLoad_encoder(const int8_t inc)
{
	if(!projectBrowse_encoder(inc, &voiceLoad_kit))
	{
		const int16_t kit = voiceLoad_kit + inc;
		voiceLoad_kit = (uint8_t)(kit < 0 ? 0 : kit >= NUM_KIT_SLOTS ? NUM_KIT_SLOTS-1 : kit);
	}
	menu_repaintAll();
}
//---------------------------------------------------
void voiceLoad_click(void)
{
	if(projectBrowse_click(0))
	{
		if(!projectBrowse.selecting && !projectBrowse.changeRow)
			voiceLoad_kit = 0;	//a project was chosen: start at its first kit
	}
	else if(projectBrowse_kitExists(voiceLoad_kit))
	{
		static uint8_t params[END_OF_SOUND_PARAMETERS];
		uint8_t engines[NUM_VOICE_SLOTS];
		const FRESULT res = projectBrowse_readKit(voiceLoad_kit, params, engines);
		if(res == FR_OK)
			kitEngines_loadVoice(voiceLoad_slot(), engines[voiceLoad_slot()], params);
		else
			menu_showSdError(res);
	}
	menu_repaintAll();
}
//---------------------------------------------------
void voiceLoad_repaint(char lines[2][17])
{
	snprintf(lines[0], 17, "Load: %s", voiceLoad_voiceNames[voiceLoad_voice]);
	projectBrowse_repaintProjectNr(lines[0]);
	if(projectBrowse_repaint(lines))
		return;

	if(!projectBrowse_kitExists(voiceLoad_kit))
		snprintf(lines[1], 17, "%2u Empty", voiceLoad_kit);
	else	//the engine the voice has in that kit at the end
		snprintf(lines[1], 17, "%2u %-8.8s  %s", voiceLoad_kit, projectBrowse_kitName(voiceLoad_kit),
				voiceLoad_engineNames[projectBrowse_kitEngine(voiceLoad_kit, voiceLoad_slot())]);
}
