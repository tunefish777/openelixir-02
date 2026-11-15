/*
 * projectBrowse.c
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 */

#include "menu/projectBrowse.h"
#include "menu/menu.h"
#include "Preset/PresetManager.h"
#include "Preset/KitEngines.h"
#include "Soundengine/Soundengine.h"
#include "Soundengine/Sequencer/sequencer.h"
#include <stdio.h>
#include <string.h>

ProjectBrowse projectBrowse;

//names and engines of another project's kits (patterns: names and exists = has steps), read
//from the SD card when it is chosen
static char projectBrowse_names[NUM_KIT_SLOTS][8];
static uint8_t projectBrowse_engines[NUM_KIT_SLOTS][NUM_VOICE_SLOTS];
static uint8_t projectBrowse_exists[NUM_KIT_SLOTS];
static uint8_t projectBrowse_named[NUM_KIT_SLOTS];		//patterns: name given in a save dialog
//---------------------------------------------------
void projectBrowse_reset(void)
{
	projectBrowse.project = menu_loadedPresetNr[CURRENT_PROJECT];
	projectBrowse.selecting = 0;
	projectBrowse.changeRow = 0;
	projectBrowse.patterns = 0;
}
//---------------------------------------------------
void projectBrowse_resetPatterns(void)
{
	projectBrowse_reset();
	projectBrowse.patterns = 1;
}
//---------------------------------------------------
uint8_t projectBrowse_isBrowsing(void)
{
	return projectBrowse.selecting || projectBrowse.changeRow || projectBrowse_isOther();
}
//---------------------------------------------------
uint8_t projectBrowse_isOther(void)
{
	return projectBrowse.project != menu_loadedPresetNr[CURRENT_PROJECT];
}
//---------------------------------------------------
static uint8_t projectBrowse_projectExists(const uint8_t project)
{
	return project == menu_loadedPresetNr[CURRENT_PROJECT] || !preset_isNameEmpty(savefileNames[project][PROJECT_NAMES]);
}
//---------------------------------------------------
uint8_t projectBrowse_encoder(const int8_t inc, uint8_t* kitNr)
{
	if(projectBrowse.selecting)
	{
		const int16_t p = projectBrowse.projectSel + inc;
		projectBrowse.projectSel = (uint8_t)(p < 0 ? 0 : p >= NUM_KIT_SLOTS ? NUM_KIT_SLOTS-1 : p);
		return 1;
	}
	if(projectBrowse.changeRow)
	{
		if(inc > 0)
		{
			projectBrowse.changeRow = 0;
			*kitNr = 0;
		}
		return 1;
	}
	if(*kitNr == 0 && inc < 0)
	{
		projectBrowse.changeRow = 1;
		return 1;
	}
	return 0;
}
//---------------------------------------------------
uint8_t projectBrowse_click(const uint8_t forSave)
{
	if(projectBrowse.selecting)
	{
		if(forSave && !projectBrowse_projectExists(projectBrowse.projectSel))
			return 1;	//kits can only be saved into an existing project
		projectBrowse.project = projectBrowse.projectSel;
		projectBrowse.selecting = 0;
		projectBrowse.changeRow = 0;
		if(projectBrowse_isOther())
		{
			//reads up to 64 files: keep the audio running meanwhile
			const uint8_t bg = soundengine_backgroundAudioBegin();
			const FRESULT res = projectBrowse.patterns ?
					preset_scanProjectPatterns(projectBrowse.project, projectBrowse_names, projectBrowse_exists, projectBrowse_named) :
					preset_scanProjectKits(projectBrowse.project, projectBrowse_names, projectBrowse_engines, projectBrowse_exists);
			if(res != FR_OK)
			{
				memset(projectBrowse_exists, 0, sizeof(projectBrowse_exists));
				memset(projectBrowse_names, 0, sizeof(projectBrowse_names));
			}
			soundengine_backgroundAudioEnd(bg);
		}
		return 1;
	}
	if(projectBrowse.changeRow)
	{
		projectBrowse.selecting = 1;
		projectBrowse.projectSel = projectBrowse.project;
		return 1;
	}
	return 0;
}
//---------------------------------------------------
const char* projectBrowse_kitName(const uint8_t kit)
{
	if(!projectBrowse_isOther())
		return savefileNames[kit][KIT_NAMES];
	return projectBrowse_exists[kit] ? projectBrowse_names[kit] : "Empty   ";
}
//---------------------------------------------------
uint8_t projectBrowse_kitExists(const uint8_t kit)
{
	return !projectBrowse_isOther() || projectBrowse_exists[kit];
}
//---------------------------------------------------
uint8_t projectBrowse_kitEngine(const uint8_t kit, const uint8_t slot)
{
	return (projectBrowse_isOther() ? projectBrowse_engines[kit][slot] : kitEngines_ram[kit][slot]) % NUM_ENGINES;
}
//---------------------------------------------------
FRESULT projectBrowse_readKit(const uint8_t kit, uint8_t* params, uint8_t* engines)
{
	if(!projectBrowse_isOther())
	{
		memcpy(params, mainParameters[kit], END_OF_SOUND_PARAMETERS);
		memcpy(engines, kitEngines_ram[kit], NUM_VOICE_SLOTS);
		return FR_OK;
	}
	const uint8_t bg = soundengine_backgroundAudioBegin();
	const FRESULT res = preset_readProjectKit(projectBrowse.project, kit, params, engines);
	soundengine_backgroundAudioEnd(bg);
	return res;
}
//---------------------------------------------------
FRESULT projectBrowse_saveKit(const uint8_t kit, char* name)
{
	const uint8_t bg = soundengine_backgroundAudioBegin();
	const FRESULT res = preset_saveProjectKit(projectBrowse.project, kit, name, activeParameters, kitEngines);
	soundengine_backgroundAudioEnd(bg);
	if(res == FR_OK)
	{
		memcpy(projectBrowse_names[kit], name, 8);
		memcpy(projectBrowse_engines[kit], kitEngines, NUM_VOICE_SLOTS);
		projectBrowse_exists[kit] = 1;
	}
	return res;
}
//---------------------------------------------------
void projectBrowse_patternName(const uint8_t nr, char* buf)
{
	if(!projectBrowse_exists[nr])
	{
		memcpy(buf, "Empty   ", 8);
		return;
	}
	if(!projectBrowse_named[nr])
	{
		memcpy(buf, "Pat     ", 8);
		buf[4] = (char)('0' + (nr+1)/10);
		buf[5] = (char)('0' + (nr+1)%10);
		return;
	}
	for(uint8_t i=0;i<8;i++)
	{
		buf[i] = projectBrowse_names[nr][i] ? projectBrowse_names[nr][i] : ' ';
	}
}
//---------------------------------------------------
uint8_t projectBrowse_patternHasSteps(const uint8_t nr)
{
	return projectBrowse_exists[nr];
}
//---------------------------------------------------
FRESULT projectBrowse_loadPattern(const uint8_t nr)
{
	const uint8_t bg = soundengine_backgroundAudioBegin();
	const FRESULT res = preset_loadOtherPattern(projectBrowse.project, nr);
	soundengine_backgroundAudioEnd(bg);
	return res;
}
//---------------------------------------------------
FRESULT projectBrowse_savePattern(const uint8_t nr, char* name, uint8_t named)
{
	const uint8_t bg = soundengine_backgroundAudioBegin();
	const FRESULT res = preset_saveProjectPattern(projectBrowse.project, nr, name, named);
	soundengine_backgroundAudioEnd(bg);
	if(res == FR_OK)
	{
		memcpy(projectBrowse_names[nr], name, 8);
		projectBrowse_named[nr] = named;
		projectBrowse_exists[nr] = seq_patternHasSteps(seq_activePattern);
	}
	return res;
}
//---------------------------------------------------
uint8_t projectBrowse_repaint(char lines[2][17])
{
	if(projectBrowse.selecting)
	{
		snprintf(lines[0], 17, "Select Project:");
		snprintf(lines[1], 17, "%2u %-8.8s", projectBrowse.projectSel, savefileNames[projectBrowse.projectSel][PROJECT_NAMES]);
		return 1;
	}
	if(projectBrowse.changeRow)
	{
		snprintf(lines[1], 17, "Change Project");
		return 1;
	}
	return 0;
}
//---------------------------------------------------
void projectBrowse_repaintProjectNr(char* line)
{
	if(projectBrowse_isOther())
		snprintf(&line[12], 5, " p%02u", (unsigned)(projectBrowse.project % 100));
}
