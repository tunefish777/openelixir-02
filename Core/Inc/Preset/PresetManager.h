/*
 * PresetManager.h
 *
 * Created: 09.05.2012 14:11:49
 *  Author: Julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */ 


#ifndef PRESETMANAGER_H_
#define PRESETMANAGER_H_

#include "main.h"
#include "string.h"
#include "fatfs.h"
#include "Parameters.h"
#include "InitKit.h"

//#define f_unmount(path) f_mount(0, path, 0)

enum
{
	KIT_NAMES,
	PATTERN_NAMES,
	SONG_NAMES,
	PROJECT_NAMES,
	NUM_SAVEFILE_NAMES,
	UNUSED_NAMES
};

extern char savefileNames[NUM_KIT_SLOTS][NUM_SAVEFILE_NAMES][8];
extern char currentFolder[13];
//extern char presetNameBuffer[8];
//extern uint8_t activePresetNr; //currently loaded kit

void preset_init(uint8_t presetNr);

#define SKIP_NEXT_KIT_AUTOLOAD 2

//-----------------------------------------------------------------
// autosave timer
// after a kit/pat is changed, wait a while, then autosave
typedef enum
{
	PRESET_KIT_TIMER,
	PRESET_PAT_TIMER,
	PRESET_STOP_PAT_TIMER
} PresetTimer;


FRESULT preset_LoadTmpKit();
FRESULT preset_LoadTmpPat();
void preset_initTmpFolder();
void preset_touchTimer(PresetTimer type);
void preset_processTimer();
FRESULT preset_clearTmpFolder();
void preset_resetAllSaveTimers();
//load all manually saved kits and patterns found in the tmp folder
FRESULT loadTmpFolderFiles();



FRESULT preset_savePatternSD(uint8_t presetNr);
FRESULT preset_loadPatternSD(uint8_t presetNr);

/** save the parameters of all 6 voices + LFO settings to a file on the SD card 
if isMorph==1 the sound will be loaded into the morph buffer
*/

//load/save drumset from/to SD card
FRESULT preset_saveDrumsetSD(uint8_t presetNr, uint8_t isMorph);
FRESULT preset_loadDrumsetSD(uint8_t presetNr, uint8_t isMorph);
//copy drumset to/from edit buffer
void preset_loadDrumsetRam(uint8_t presetNr, uint8_t isMorph, uint8_t sendImmediate);
void preset_saveDrumsetRam(uint8_t presetNr, uint8_t isMorph);


FRESULT preset_saveGlobals();
uint8_t preset_loadGlobals();
void preset_createGlobals();

FRESULT preset_loadLastUsedProjectNrFromSD(uint8_t* projNr);
FRESULT preset_saveLastUsedProjectNrToSD(uint8_t projNr);

void preset_loadDefaultGlobals();

FRESULT preset_saveProject(uint8_t presetNr);
FRESULT preset_loadProject(uint8_t presetNr, uint8_t clearTmp);
FRESULT preset_createProject(uint8_t presetNr);
void preset_loadProjectNames(uint8_t projectNr);

//Kits of a project folder on the SD card, without changing the current folder
//or anything loaded. Kit files are NN-NAME.snd (or the older pNNN.snd).
//names/engines of all kits of a project (exists[kit] = 0 for kits without a file)
FRESULT preset_scanProjectKits(uint8_t projectNr, char names[][8], uint8_t engines[][6], uint8_t* exists);
//parameters (END_OF_SOUND_PARAMETERS) and engines of one kit
FRESULT preset_readProjectKit(uint8_t projectNr, uint8_t kitNr, uint8_t* params, uint8_t* engines);
//save a kit into an existing project folder (replaces the kit's old file)
FRESULT preset_saveProjectKit(uint8_t projectNr, uint8_t kitNr, char* name, const uint8_t* params, const uint8_t* engines);
//the active kit becomes these parameters and engines (a kit from another
//project); nothing is written (no RAM kit, no autosave) until the user saves it
void preset_loadKitData(const uint8_t* params, const uint8_t* engines, const char* name);
//the name of the active kit: the kit it was loaded from (8 characters)
const char* preset_activeKitName(void);

FRESULT preset_loadSongSD(uint8_t songNr);
FRESULT preset_saveSongSD(uint8_t songNr);

//char* preset_loadName(uint8_t presetNr, uint8_t nameBufferIdx, uint8_t what);

/** save a pattern set to the sd card */


FRESULT preset_savePatternRAM(uint8_t presetNr);
FRESULT preset_loadPatternRAM(uint8_t presetNr);


//uint8_t preset_loadPatternTemp(uint8_t presetNr);
//void preset_savePatternTemp(uint8_t presetNr);
/** morph pattern linear to �pattern buffer 2*/
void preset_morph(uint8_t morph);
void preset_morph_tick(); //handler for non blocking morph
uint8_t preset_getMorphValue(uint16_t index, uint8_t morph);
void preset_sendDrumsetParameters();
void preset_sendDrumsetParametersImmediate();

uint8_t preset_isNameEmpty(char* name);

#endif /* PRESETMANAGER_H_ */
