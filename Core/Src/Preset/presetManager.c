/*
 * presetManager.c
 *
 * Created: 09.05.2012 16:06:19
 *  Author: Julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */ 
//#include "../config.h"
#include <frontPanelParser_ui.h>
#include "Preset/PresetManager.h"
#include "Preset/KitEngines.h"
//#include "../Hardware/SD/ff.h"
#include "ff.h"
#include <stdio.h>
#include "menu/CcNr2Text.h"
#include "menu/menu.h"
#include <strings.h>
//#include <util/delay.h>
#include "lcd.h"
//#include <avr/pgmspace.h>
#include <stdlib.h>
//#include <util/atomic.h>
#include "IO/uart.h"
#include "IO/DIO.h"

#include "Soundengine/MIDI/MidiParser.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "Soundengine/Soundengine.h"




//#include "../Hardware/timebase.h"

//FATFS SDFatFS;		/* File system object for the logical drive */
//FIL SDFile;		/* place to hold 1 file*/
static uint8_t morph_state = 0; //flag for non blocking morph
static uint8_t morph_index = 0;
char savefileNames[NUM_KIT_SLOTS][NUM_SAVEFILE_NAMES][8];

char currentFolder[13];

//project save: folder suffixes (8.3 names: /projNNn, /projNNo)
#define PROJ_NEW_SUFFIX "n"
#define PROJ_OLD_SUFFIX "o"
//set while a project is written into its new (empty) folder: no old files to remove
static uint8_t preset_writingNewFolder = 0;

//char presetNameBuffer[8];
//char currentName[8];
//uint8_t activePresetNr = 0;

#define FILE_VERSION 2

/*
#define NUM_TRACKS 7
#define NUM_PATTERN 8*/
#define STEPS_PER_PATTERN NUM_STEPS //64


#define FEXT_SOUND 	0
#define FEXT_PAT 	1
#define FEXT_ALL 	2
#define FEXT_PROJ 	3
#define FEXT_SONG 	4



// fill buffer (length 9 total) with a filename. type is one of above
// eg p001.snd
static void preset_makeFileNameOld(char *buf, uint8_t num, uint8_t type);

//load uses wildcard to find a name of the pattern 00-*.ext
static FRESULT preset_makeFileNameLoad(char *buf, uint8_t num, uint8_t type);
static FRESULT preset_removeOtherFiles(uint8_t num, uint8_t type, const char *keep);
static uint8_t preset_readPatternData(uint8_t patternNr, uint8_t dataVersion);
//save uses the kit name to create a filename eg 00-NAME.ext
static void preset_makeFileNameSave(char *buf, uint8_t num, uint8_t type, char *name);

//static void preset_makeFileNameTemp(char *buf, uint8_t num, uint8_t type);


#define PRESET_SAVE_TIME 2000

uint32_t preset_kitTimer = 0;
uint32_t preset_patTimer = 0;

uint8_t preset_kitDirty = 0;
uint8_t preset_patDirty = 0;

//one pattern as stored in a pattern file (and tmp.pat), read completely before it is used
typedef struct PatternFileDataStruct
{
	Step steps[NUM_TRACKS][NUM_STEPS];
	StepTiming timing[NUM_TRACKS][NUM_STEPS];
	uint16_t mainSteps[NUM_BARS][NUM_TRACKS];
	uint8_t length[NUM_TRACKS];
	uint8_t shuffle;
	uint8_t kitNr;
}PatternFileData;
static PatternFileData preset_patternBuffer;

//values read from files are used as indices: keep them in range
//track length: 0 is read as 16 by the sequencer, more than NUM_STEPS steps don't exist
static uint8_t preset_validLength(const uint8_t length)
{
	return length > NUM_STEPS ? NUM_STEPS : length;
}
static void preset_validateKitNr(uint8_t patternNr)
{
	if(seq_patternSet.kitNr[patternNr] >= NUM_KIT_SLOTS)
		seq_patternSet.kitNr[patternNr] = 0;
}

//Writing a file: the first error of all writes, and a short write (card full)
//counts as one; preset_fcloseWrite() closes the file and returns that error, or
//the result of closing it (which writes the last data). So every save closes its
//file (FatFs allows one open file at a time: a file left open blocked the card
//until a reboot) and reports a failed or incomplete write.
static FRESULT preset_writeResult = FR_OK;

static FRESULT preset_fopenWrite(const char* path)
{
	preset_writeResult = f_open((FIL*)&SDFile, path, FA_CREATE_ALWAYS | FA_WRITE);
	return preset_writeResult;
}

static FRESULT preset_fwrite(FIL* fp, const void* buf, UINT btw, UINT* bw)
{
	if(preset_writeResult != FR_OK)
		return preset_writeResult;
	preset_writeResult = f_write(fp, buf, btw, bw);
	if(preset_writeResult == FR_OK && *bw != btw)
		preset_writeResult = FR_DENIED;	//card full
	return preset_writeResult;
}

static FRESULT preset_fcloseWrite(void)
{
	const FRESULT res = f_close((FIL*)&SDFile);
	return preset_writeResult != FR_OK ? preset_writeResult : res;
}

//name of a kit loaded from another project (it has no slot in the active project)
static char preset_otherKitName[8];
static uint8_t preset_otherKitLoaded = 0;


//the engines of the RAM kit morphParameters points to (the active kit's if it
//doesn't point to one yet: it is NULL during the first kit loads at boot)
static uint8_t* preset_morphEngines()
{
	const uint8_t* first = &mainParameters[0][0];
	if(morphParameters < first || morphParameters >= first + sizeof(mainParameters))
		return kitEngines;
	return kitEngines_ram[(morphParameters - first) / END_OF_SOUND_PARAMETERS];
}
//is par an engine specific parameter of a slot that has another engine in the morph kit?
static uint8_t preset_isMorphEngineMismatch(uint16_t par)
{
	const uint8_t slot = kitEngines_rolePar2Slot(par);
	return slot < NUM_VOICE_SLOTS && kitEngines[slot] != preset_morphEngines()[slot];
}
//write the engine block after the kit parameters
static FRESULT preset_writeEngines(const uint8_t* engines)
{
	uint8_t block[KIT_ENGINES_BLOCK_SIZE];
	unsigned int bytesWritten;
	return preset_fwrite((FIL*)&SDFile, block, kitEngines_encode(engines, block), &bytesWritten);
}
//read the engine block after the kit parameters (stock engines if there is none)
static void preset_readEngines(uint8_t* engines)
{
	uint8_t block[32];
	unsigned int bytesRead = 0;
	if(f_read((FIL*)&SDFile, block, sizeof(block), &bytesRead) != FR_OK)
		bytesRead = 0;
	kitEngines_decode(block, (uint16_t)bytesRead, engines);
}

void preset_initTmpFolder()
{
	char folderName[13];
	sprintf(folderName, "/tmp");

	FRESULT res;
	FILINFO fno;

	res = f_stat(folderName, &fno);
	if(res == FR_NO_FILE)
	{
		res = f_mkdir(folderName);
		if(res == FR_OK)
		{
			f_chdir(folderName);
		}
	}
	else
	{
		f_chdir(folderName);
	}
}

void preset_resetAllSaveTimers()
{
	preset_kitDirty = 0;
	preset_patDirty = 0;
}

FRESULT loadTmpFolderFiles()
{
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
	HAL_TIM_Base_Start_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif

	FRESULT res;
	//check tmp folder for *.snd and *.pat files and load them to RAM
	for(uint8_t i=0;i<NUM_KIT_SLOTS;i++)
	{

		//FR_NO_FILE just means an incomplete project and is no reason to abort!
		res = preset_loadDrumsetSD(i, 0);
		if ((res != FR_OK) && (res != FR_NO_FILE))
		{
			lcd_process_display_interrupt = 1;
			preset_initTmpFolder();
			f_close((FIL*)&SDFile);
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
			return res;
		}
		res = preset_loadPatternSD(i);
		if ((res != FR_OK) && (res != FR_NO_FILE))
		{
			f_close((FIL*)&SDFile);
			lcd_process_display_interrupt = 1;
			preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
			return res;
		}
		res = preset_loadSongSD(i);
		if ((res != FR_OK) && (res != FR_NO_FILE))
		{
			f_close((FIL*)&SDFile);
			lcd_process_display_interrupt = 1;
			preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
			return res;
		}
	}

	return FR_OK;
}

void preset_touchTimer(PresetTimer type)
{
	if(type==PRESET_KIT_TIMER)
	{
		preset_kitTimer = HAL_GetTick();
		preset_kitDirty = 1;
	}
	else if(type==PRESET_PAT_TIMER)
	{
		preset_patTimer = HAL_GetTick();
		preset_patDirty = 1;
	}
	else if(type==PRESET_STOP_PAT_TIMER)
	{
		//abort pattern save timer, since seq is running again...
		preset_patDirty = 0;
	}

};


FRESULT preset_saveTmpKit()
{
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
	HAL_TIM_Base_Start_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
	FRESULT res;
	unsigned int bytesWritten;

	//open the file
	res = preset_fopenWrite("tmp.snd");
	if(res != FR_OK)
	{
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
		return res;
	}

	//write current preset nr to file
	res = preset_fwrite((FIL*)&SDFile,(void*)&menu_loadedPresetNr[CURRENT_KIT],1,&bytesWritten);
	if(res != FR_OK)
	{
		f_close((FIL*)&SDFile);
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
		return res;
	}
	//write the preset data
	preset_fwrite((FIL*)&SDFile,activeParameters,END_OF_SOUND_PARAMETERS,&bytesWritten);
	preset_writeEngines(kitEngines);

	//close the file
	res = preset_fcloseWrite();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
	return res;
}

FRESULT preset_LoadTmpKit()
{
	FRESULT res;
	unsigned int bytesRead;
	uint8_t kitNr = 0;

	//open the file
	res = f_open((FIL*)&SDFile,"tmp.snd",FA_OPEN_EXISTING | FA_READ);
	if(res != FR_OK) return res;

	//first the preset nr
	res=f_read((FIL*)&SDFile,(void*)&kitNr,1,&bytesRead);
	if(bytesRead != 1) {
		res=FR_DISK_ERR;
		goto closeFile;
	}

	if(kitNr >= NUM_KIT_SLOTS)
		kitNr = 0;
	menu_selectedPresetNr[SAVE_TYPE_KIT] = kitNr;
	menu_loadedPresetNr[CURRENT_KIT] = kitNr;


	//read the file content
		bytesRead=1;
		int16_t i;
		uint8_t *para;
		bytesRead = 0;

		para=activeParameters;

		res=f_read((FIL*)&SDFile, para,END_OF_SOUND_PARAMETERS, &bytesRead);
		if(res==FR_OK) {
			// set to 0 for any that were not read from the file
			if(END_OF_SOUND_PARAMETERS-bytesRead)
				memset(para+bytesRead,0,END_OF_SOUND_PARAMETERS-bytesRead);
			preset_readEngines(kitEngines);

			//special case mod targets. normalize.
			const uint8_t nmt=getNumModTargets();
			for(i=0;i<6;i++) {
				// since I've changed the meaning of these, I'll ensure that it's valid for kits saved prior to the
				// change
				if(para[PAR_VEL_DEST_1+i] >= nmt )
					para[PAR_VEL_DEST_1+i] = 0;
				if(para[PAR_TARGET_LFO1+i] >= nmt )
					para[PAR_TARGET_LFO1+i] = 0;
			}
		}


	closeFile:
		//close the file handle
		f_close((FIL*)&SDFile);

		// update the soundengine with the new parameters
		if(res==FR_OK) {
			preset_sendDrumsetParameters();
			return FR_OK;
		}

		return res;
}

FRESULT preset_LoadTmpPat()
{

	uint8_t success=1; // start off succeeding
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
	HAL_TIM_Base_Start_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
	//filename in 8.3  format
		//uint8_t fileVersion = 1;
		UINT bytesRead;
		FRESULT res;



		res = f_open((FIL*)&SDFile,"tmp.pat",FA_OPEN_EXISTING | FA_READ);
		if(res != FR_OK)
		{
		#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
					HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
		#else

		#endif
				return res;
		}

		//read current preset nr
		res = f_read((FIL*)&SDFile,(void*)&menu_playedPattern,1,&bytesRead);
		if(bytesRead != 1)
		{
			success = 0;
			goto closeFile;
		}

		if(menu_playedPattern >= NUM_PATTERN)
		{
			menu_playedPattern = 0;
			success = 0;
			goto closeFile;
		}

		menu_shownPattern = menu_playedPattern;
		seq_activePattern = menu_playedPattern;
		seq_pendingPattern = menu_playedPattern;
		menu_selectedPresetNr[SAVE_TYPE_PATTERN] = menu_playedPattern;

		frontPanel_sendData(SEQ_CC,SEQ_SET_SHOWN_PATTERN,menu_playedPattern);

		seq_copyPatternToBackupBuffer(seq_activePattern);
		//frontParser_shownPattern = menu_playedPattern;
		//same layout as a V3 pattern file (without name and version tag)
		success = preset_readPatternData(menu_playedPattern, 3);

	closeFile:
		f_close((FIL*)&SDFile);

		#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
					HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
		#else

		#endif


		// return success or failure
		if(success) return FR_OK;
		else return FR_INT_ERR;


}

FRESULT preset_saveTmpPat()
{

	//don't save invalid pattern
	if(menu_playedPattern >= NUM_PATTERN) return FR_INVALID_PARAMETER;
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
	HAL_TIM_Base_Start_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif

	FRESULT res;
	UINT bytesWritten;
	uint8_t presetNr = menu_playedPattern;
	//lcd_clear();
	//lcd_home();
	//lcd_string(PSTR("Saving pattern"));

	//open the file
	res = preset_fopenWrite("tmp.pat");
	if(res != FR_OK)
	{
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
		return res;
	}

	//write current preset nr to file
	res = preset_fwrite((FIL*)&SDFile,(void*)&menu_playedPattern,1,&bytesWritten);
	if(res != FR_OK)
	{
		f_close((FIL*)&SDFile);
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
		return res;
	}

	//write pattern data
	//preset_writePatternData(presetNr);
#if 0
#else

		uint8_t length;

		//---------------  write the step data (note, vol, prob, autom.) ---------------
		for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
		{
			preset_fwrite((FIL*)&SDFile,(const void*)&seq_patternSet.seq_subStepPattern[presetNr][trk][0],sizeof(Step)*STEPS_PER_PATTERN,&bytesWritten);

			preset_fwrite((FIL*)&SDFile,(const void*)&seq_patternSet.seq_stepTiming[presetNr][trk][0],sizeof(uint8_t)*STEPS_PER_PATTERN,&bytesWritten);
		}

		//--------------- now the main step data ---------------
		for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
		{
			for(uint8_t bar=0;bar<NUM_BARS; bar++)
			{
				const uint16_t mainStepData = seq_patternSet.seq_mainSteps[bar][presetNr][trk];
				preset_fwrite((FIL*)&SDFile,(const void*)&mainStepData,sizeof(uint16_t),&bytesWritten);
			}
		}

		//----- shuffle setting ------


		//preset_fwrite((FIL*)&SDFile,(const void*)&activeParameters[PAR_SHUFFLE],sizeof(uint8_t),&bytesWritten);
		preset_fwrite((FIL*)&SDFile,(const void*)&seq_patternSet.shuffle[presetNr],sizeof(uint8_t),&bytesWritten);

		//----- pattern/track lengths ------
		// we reuse the same call from above (when saving main step data)
		// but we only use the length info retrieved. We want to store it at the end
		// to avoid breaking compatibility with save file


		for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
		{
				length = seq_patternSet.length[presetNr][trk] ;//seq_patternSet.seq_mainSteps[bar][pat][trk];
				preset_fwrite((FIL*)&SDFile,(const void*)&length,sizeof(uint8_t),&bytesWritten);
		}

		//write associated kit nr
		preset_fwrite((FIL*)&SDFile,&seq_patternSet.kitNr[presetNr],sizeof(uint8_t),&bytesWritten);

#endif

	//close the file
	res = preset_fcloseWrite();

	//reset the lcd
//	menu_repaintAll();

#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif

	return res;
}

/*------------------------------------------------------------/
/ Delete a sub-directory even if it contains any file
/-------------------------------------------------------------/
/ The delete_node() function is for R0.12+.
/ It works regardless of FF_FS_RPATH.
*/


FRESULT delete_node (
    TCHAR* path,    /* Path name buffer with the sub-directory to delete */
    UINT sz_buff,   /* Size of path name buffer (items) */
    FILINFO* fno    /* Name read buffer */
)
{
    UINT i, j;
    FRESULT fr;
    DIR dir;


    fr = f_opendir(&dir, path); /* Open the sub-directory to make it empty */
    if (fr != FR_OK) return fr;

    for (i = 0; path[i]; i++) ; /* Get current path length */
    path[i++] = _T('/');

    for (;;) {
        fr = f_readdir(&dir, fno);  /* Get a directory item */
        if (fr != FR_OK || !fno->fname[0]) break;   /* End of directory? */
        j = 0;
        do {    /* Make a path name */
            if (i + j >= sz_buff) { /* Buffer over flow? */
                fr = 100; break;    /* Fails with 100 when buffer overflow */
            }
            path[i + j] = fno->fname[j];
        } while (fno->fname[j++]);
        if (fno->fattrib & AM_DIR) {    /* Item is a sub-directory */
            fr = delete_node(path, sz_buff, fno);
        } else {                        /* Item is a file */
            fr = f_unlink(path);
        }
        if (fr != FR_OK) break;
    }

    path[--i] = 0;  /* Restore the path name */
    f_closedir(&dir);

    if (fr == FR_OK) fr = f_unlink(path);  /* Delete the empty sub-directory */
    return fr;
}

FRESULT preset_clearTmpFolder()
{
	FILINFO fno;
	FRESULT fr;
	TCHAR buff[256];

	f_chdir("/");

	/* Directory to be deleted */
	sprintf(buff, "/tmp");

	fr = delete_node(buff, sizeof buff / sizeof buff[0], &fno);

	preset_initTmpFolder();

	return fr;
}


void preset_processTimer()
{
	if(preset_patDirty == 1)
	{
		if(HAL_GetTick()-preset_patTimer >= PRESET_SAVE_TIME )
		{
			preset_patDirty = 0;
			preset_saveTmpPat();
		}
	}

	if(preset_kitDirty == 1)
	{
		if(HAL_GetTick()-preset_kitTimer >= PRESET_SAVE_TIME )
		{
			preset_kitDirty = 0;
			preset_saveTmpKit();
		}
	}
}


/*------------------------------------------------------------/
/ Remove all contents of a directory
/ This function works regardless of FF_FS_RPATH.
/------------------------------------------------------------*/




//size of the working buffer for empty_directory(): it appends "/" + an 8.3
//name (12 characters, no long file names) per directory level
#define EMPTY_DIR_PATH_LEN	64

FRESULT empty_directory (
    char* path      /* Working buffer (EMPTY_DIR_PATH_LEN) filled with start directory */
)
{
	FILINFO fno;
    UINT i, j;
    FRESULT fr;
    DIR dir;

    fr = f_opendir(&dir, path);
    if (fr == FR_OK) {
        for (i = 0; path[i]; i++) ;
        path[i++] = '/';
        for (;;) {
            fr = f_readdir(&dir, &fno);
            if (fr != FR_OK || !fno.fname[0]) break;
            if (_FS_RPATH && fno.fname[0] == '.') continue;
            if (i + sizeof(fno.fname) > EMPTY_DIR_PATH_LEN) { fr = FR_INVALID_NAME; break; }
            j = 0;
            do
                path[i+j] = fno.fname[j];
            while (fno.fname[j++]);
            if (fno.fattrib & AM_DIR) {
                fr = empty_directory(path);
                if (fr != FR_OK) break;
            }
            fr = f_unlink(path);
            if (fr != FR_OK) break;
        }
        path[--i] = '\0';
        f_closedir(&dir);
    }

    return fr;
}
//----------------------------------
//removes a folder with its content; a missing folder is no error
static FRESULT preset_removeFolder(const char* folder)
{
	char path[EMPTY_DIR_PATH_LEN];
	FILINFO fno;
	FRESULT res = f_stat(folder, &fno);
	if(res == FR_NO_FILE)
		return FR_OK;
	if(res != FR_OK)
		return res;
	strcpy(path, folder);
	res = empty_directory(path);
	if(res != FR_OK)
		return res;
	return f_unlink(folder);
}
//----------------------------------
//a project save writes into /projNN + PROJ_NEW_SUFFIX, then renames /projNN to /projNN + PROJ_OLD_SUFFIX,
//the new folder to /projNN and removes the old one. If the unit was switched off between the
//renames, /projNN is missing: the new folder is complete at that point, so finish the save.
//(The old folder is only taken back if the new one is gone.)
static void preset_projectFolderName(char* folder, uint8_t projectNr)
{
	char other[12];
	FILINFO fno;
	sprintf(folder, "/proj%02d", projectNr);
	if(f_stat(folder, &fno) != FR_NO_FILE)
		return;
	sprintf(other, "/proj%02d" PROJ_NEW_SUFFIX, projectNr);
	if(f_rename(other, folder) == FR_OK)
		return;
	sprintf(other, "/proj%02d" PROJ_OLD_SUFFIX, projectNr);
	f_rename(other, folder);
}
//----------------------------------

uint8_t preset_isNameEmpty(char* name)
{
	uint8_t isEmpty = 1;

	if( (name[0] != 'E') && (name[0] != 0) ) isEmpty = 0;
	if( (name[1] != 'm') && (name[1] != 0) ) isEmpty = 0;
	if( (name[2] != 'p') && (name[2] != 0) ) isEmpty = 0;
	if( (name[3] != 't') && (name[3] != 0) ) isEmpty = 0;
	if( (name[4] != 'y') && (name[4] != 0) ) isEmpty = 0;
	if( (name[5] != 0) ) isEmpty = 0;
	if( (name[6] != 0) ) isEmpty = 0;
	if( (name[7] != 0) ) isEmpty = 0;


	return isEmpty;
}
//----------------------------------------------------
//load init kit
void preset_init(uint8_t presetNr)
{

	//const uint8_t presetNr = menu_selectedPresetNr[SAVE_TYPE_KIT];

	//first the preset name
	memcpy(savefileNames[presetNr][KIT_NAMES],initKit,8*sizeof(uint8_t));


	if(preset_isNameEmpty(savefileNames[presetNr][KIT_NAMES]))
	{
		sprintf((void*)savefileNames[presetNr][KIT_NAMES], "Empty");
	}


	//then the data
	//read the file content

	int16_t i;
	uint8_t *para;


	para=mainParameters[presetNr];

	//initKit = 8 name bytes + parameters; parameters missing in it are 0
	for(i=0;i<END_OF_SOUND_PARAMETERS; i++)
	{
		para[i] = i < INIT_KIT_SIZE - 8 ? initKit[8 + i] : 0;
	}

	//special case mod targets. normalize.
	const uint8_t nmt=getNumModTargets();
	for(i=0;i<6;i++) {
		// since I've changed the meaning of these, I'll ensure that it's valid for kits saved prior to the
		// change
		if(para[PAR_VEL_DEST_1+i] >= nmt )
			para[PAR_VEL_DEST_1+i] = 0;
		if(para[PAR_TARGET_LFO1+i] >= nmt )
			para[PAR_TARGET_LFO1+i] = 0;
	}

	kitEngines_setStock(kitEngines_ram[presetNr]);

	//copy parameters to edit buffer
	memcpy(activeParameters, mainParameters[presetNr], sizeof(uint8_t) * END_OF_SOUND_PARAMETERS);
	memcpy(kitEngines, kitEngines_ram[presetNr], NUM_VOICE_SLOTS);
	preset_sendDrumsetParameters();
	//frontPanel_sendData(PRESET,PRESET_LOAD,presetNr);

}

//----------------------------------------------------
static void preset_writeDrumsetData(uint8_t presetNr, uint8_t isMorph)
{
	uint16_t i;
	unsigned int bytesWritten;

	if(isMorph)
	{
		for(i=0;i<END_OF_SOUND_PARAMETERS;i++)
		{
			uint8_t value;
			//Mod targets are not morphed!!!
			if( (i >= PAR_VEL_DEST_1) && (i <= PAR_VEL_DEST_6) )
			{
				value = activeParameters[i];
			} else if( (i >= PAR_TARGET_LFO1) && (i <= PAR_TARGET_LFO6) )
			{
				value = activeParameters[i];
			} else if( (i >= PAR_VOICE_LFO1) && (i <= PAR_VOICE_LFO6) )
			{
				value = activeParameters[i];
			} 
			else 
			{
				value = preset_getMorphValue(i,activeParameters[PAR_MORPH]);
			}					
			preset_fwrite((FIL*)&SDFile,&value,1,&bytesWritten);
		}
		//a morphed kit keeps the engines of the source kit
		preset_writeEngines(kitEngines);
	} else {
		for(i=0;i<END_OF_SOUND_PARAMETERS;i++)
		{
			preset_fwrite((FIL*)&SDFile,&mainParameters[presetNr][i],1,&bytesWritten);
		}
		preset_writeEngines(kitEngines_ram[presetNr]);
	}
}

//----------------------------------------------------
void preset_loadDrumsetRam(uint8_t presetNr, uint8_t isMorph, uint8_t sendImmediate)
{
	soundengine_process();
	if(isMorph)
	{
		morphParameters = mainParameters[presetNr];
	}
	else
	{
		//copy parameters to edit buffer
		memcpy(activeParameters, mainParameters[presetNr], sizeof(uint8_t) * END_OF_SOUND_PARAMETERS);
		memcpy(kitEngines, kitEngines_ram[presetNr], NUM_VOICE_SLOTS);
		preset_otherKitLoaded = 0;
		//if(activeParameters[PAR_PATTERN_MODE] = 1)
		//load asociated morph kit as well

		// morphParameters = mainParameters[activeParameters[PAR_MORPH_KIT_DEST]];


	}
	soundengine_process();

	if(sendImmediate)
	{
		preset_sendDrumsetParametersImmediate();
	}
	else
	{
		//activePresetNr = menu_currentPresetNr[menu_saveOptions.category];
		preset_sendDrumsetParameters();
	}
}
//----------------------------------------------------
void preset_saveDrumsetRam(uint8_t presetNr, uint8_t isMorph)
{
	if(isMorph)
	{
		for(uint16_t i=0;i<END_OF_SOUND_PARAMETERS;i++)
		{
			uint8_t value;
			//Mod targets are not morphed!!!
			if( (i >= PAR_VEL_DEST_1) && (i <= PAR_VEL_DEST_6) )
			{
				value = activeParameters[i];
			} else if( (i >= PAR_TARGET_LFO1) && (i <= PAR_TARGET_LFO6) )
			{
				value = activeParameters[i];
			} else if( (i >= PAR_VOICE_LFO1) && (i <= PAR_VOICE_LFO6) )
			{
				value = activeParameters[i];
			}
			else
			{
				value = preset_getMorphValue(i,activeParameters[PAR_MORPH]);
			}
			//preset_fwrite((FIL*)&SDFile,&value,1,&bytesWritten);
			mainParameters[presetNr][i] = value;
		}
	}
	else
	{
		//copy edit buffer to parameter array
		memcpy(mainParameters[presetNr], activeParameters, sizeof(uint8_t) * END_OF_SOUND_PARAMETERS);
		//the active kit now is this slot (also if it came from another project)
		preset_otherKitLoaded = 0;
	}
	//a morphed kit keeps the engines of the source kit
	memcpy(kitEngines_ram[presetNr], kitEngines, NUM_VOICE_SLOTS);
}
//----------------------------------------------------
FRESULT preset_saveDrumsetSD(uint8_t presetNr, uint8_t isMorph)
{
#if USE_SD_CARD
	FRESULT res;
	unsigned int bytesWritten;
	//filename in 8.3  format
	char filename[13];
	preset_makeFileNameSave(filename,presetNr,FEXT_SOUND, savefileNames[presetNr][KIT_NAMES]);
	//sprintf(filename,"p%03d.snd",presetNr);



	//open the file
	res = preset_fopenWrite(filename);
	if(res != FR_OK) return res;
	
	//write preset name to file

	res = preset_fwrite((FIL*)&SDFile,(void*)savefileNames[presetNr][KIT_NAMES],8,&bytesWritten);
	//write the preset data
	preset_writeDrumsetData(presetNr, isMorph);
	
	//close the file
	res = preset_fcloseWrite();
	if(res == FR_OK)
		res = preset_removeOtherFiles(presetNr, FEXT_SOUND, filename);

	return res;
#else
#endif
};

//----------------------------------------------------
static void preset_writeGlobalData()
{
	unsigned int bytesWritten;
	int i;
	for(i=PAR_BEGINNING_OF_GLOBALS;(i<NUM_PARAMS);i++)
	{
		preset_fwrite((FIL*)&SDFile,&activeParameters[i],1,&bytesWritten);
	}
}

//----------------------------------------------------
FRESULT preset_saveGlobals()
{
#if USE_SD_CARD
	FRESULT res;
/*
	res = f_mount((FATFS*)&SDFatFS, (TCHAR const*)SDPath,1);
	if( res != FR_OK)
	{
		return res;
	}
	*/
	//open the file
	res = preset_fopenWrite("glo.cfg");
	if(res != FR_OK) return res;
	preset_writeGlobalData();

	//close the file
	res = preset_fcloseWrite();

	//f_unmount((TCHAR const*)SDPath);
	return res;
#else
#endif	
}
//----------------------------------------------------
// returns 1 on success
static uint8_t preset_readGlobalData()
{
#if USE_SD_CARD

	UINT bytesRead;
#if 0
	int i;
	for(i=PAR_BEGINNING_OF_GLOBALS;(i<NUM_PARAMS) &&( bytesRead!=0);i++) {
		f_read((FIL*)&SDFile,&activeParameters[i],1,&bytesRead);
		if(!bytesRead)
			return 0;
	}
	if(!bytesRead)
		return 0;
	return 1;
#else
	const uint8_t numGlobals = NUM_PARAMS - PAR_BEGINNING_OF_GLOBALS;
	//settings files from the stock firmware end before the OpenElixir-02 settings:
	//those keep their defaults
	const uint8_t numStockGlobals = PAR_MIXER_LIMITER - PAR_BEGINNING_OF_GLOBALS;
	//read into a buffer first, so a damaged file doesn't leave half of the settings overwritten
	uint8_t data[NUM_PARAMS - PAR_BEGINNING_OF_GLOBALS];
	if(f_read((FIL*)&SDFile,data,numGlobals,&bytesRead) != FR_OK || bytesRead < numStockGlobals) return 0;
	memcpy(&activeParameters[PAR_BEGINNING_OF_GLOBALS],data,bytesRead);
	if(bytesRead < numGlobals)
	{
		activeParameters[PAR_MIXER_LIMITER] = 1;
	}
	return 1;

#endif
#endif
}
//----------------------------------------------------
uint8_t preset_loadGlobals()
{


	FRESULT res;
/*
	res = f_mount((FATFS*)&SDFatFS, (TCHAR const*)SDPath,1);
	if( res != FR_OK)
	{
		return res;
	}
	*/

	//open the file
	res = f_open((FIL*)&SDFile,"glo.cfg",FA_OPEN_EXISTING | FA_READ);
	
	if(res!=FR_OK)
		return 0; //file open error... maybe the file does not exist?

	if(!preset_readGlobalData())
	{
		f_close((FIL*)&SDFile);
		return 0;
	}
	f_close((FIL*)&SDFile);
	menu_sendAllGlobals();
	
	//f_unmount((TCHAR const*)SDPath);

	return 1;

}




//----------------------------------------------------
void preset_loadDefaultGlobals()
{
	activeParameters[PAR_BPM] = 120;

	activeParameters[PAR_MIDI_CHAN_1] = 1;
	activeParameters[PAR_MIDI_CHAN_2] = 1;
	activeParameters[PAR_MIDI_CHAN_3] = 1;
	activeParameters[PAR_MIDI_CHAN_4] = 1;
	activeParameters[PAR_MIDI_CHAN_5] = 1;
	activeParameters[PAR_MIDI_CHAN_6] = 1;
	activeParameters[PAR_MIDI_CHAN_7] = 1;

	activeParameters[PAR_MIDI_NOTE1] = 48;
	activeParameters[PAR_MIDI_NOTE2] = 48;
	activeParameters[PAR_MIDI_NOTE3] = 48;
	activeParameters[PAR_MIDI_NOTE4] = 48;
	activeParameters[PAR_MIDI_NOTE5] = 48;
	activeParameters[PAR_MIDI_NOTE6] = 48;
	activeParameters[PAR_MIDI_NOTE7] = 48;


	activeParameters[PAR_FETCH] = 0;
	activeParameters[PAR_FOLLOW] = 0;

	activeParameters[PAR_QUANTISATION] = 0;

	activeParameters[PAR_SCREENSAVER_ON_OFF] = 1;

	activeParameters[PAR_MIDI_ROUTING] = 0;
	activeParameters[PAR_MIDI_FILT_TX] = 15;	//all
	activeParameters[PAR_MIDI_FILT_RX] = 15; //all

	activeParameters[PAR_PRESCALER_CLOCK_IN] = 0; //all
	activeParameters[PAR_PRESCALER_CLOCK_OUT1] = 0; //all
	activeParameters[PAR_PRESCALER_CLOCK_OUT2] = 0; //all
	activeParameters[PAR_TRIG_GATE_MODE] = 0; //all

	activeParameters[PAR_BAR_RESET_MODE] = 1; //all
	activeParameters[PAR_MIDI_CHAN_GLOBAL] = 0; //all

	activeParameters[PAR_PATTERN_MODE] = 1; //auto

	activeParameters[PAR_MIXER_LIMITER] = 1; //on

}
//----------------------------------------------------
void preset_createGlobals()
{
	preset_loadDefaultGlobals();
	preset_saveGlobals();
}
//----------------------------------------------------
FRESULT preset_loadLastUsedProjectNrFromSD(uint8_t* projNr)
{
    FRESULT res;

    *projNr = 0;
    unsigned int bytesRead;

    res = f_open((FIL*)&SDFile,"/proj.idx",FA_OPEN_EXISTING | FA_READ);
    if(res != FR_OK)
    {
    	//close the file handle
    	f_close((FIL*)&SDFile);
    	return res;
    }

    res=f_read((FIL*)&SDFile, projNr,1, &bytesRead);
	//close the file handle
	f_close((FIL*)&SDFile);
	//used as index into the project names (64 projects)
	if(*projNr >= NUM_KIT_SLOTS)
		*projNr = 0;


	return res;
}
//----------------------------------------------------
FRESULT preset_saveLastUsedProjectNrToSD(uint8_t projNr)
{
#if USE_SD_CARD
    FRESULT res;

	//open the file
	res = preset_fopenWrite("/proj.idx");
	if(res != FR_OK)
	{
		//close the file handle
		f_close((FIL*)&SDFile);
		return res;
	}

	unsigned int bytesWritten;
	res = preset_fwrite((FIL*)&SDFile,&projNr,1,&bytesWritten);
	if(res != FR_OK)
	{
		//close the file handle
		f_close((FIL*)&SDFile);
		return res;
	}

	//close the file
	res = preset_fcloseWrite();

	//f_unmount((TCHAR const*)SDPath);
	return res;
#else
#endif
}
//----------------------------------------------------
// read the data into either the main parameters or the morph parameters.
// fix some values, and set values to 0 that were not read in
static FRESULT preset_readDrumsetData(uint8_t presetNr, uint8_t isMorph)
{
	
#if USE_SD_CARD		
	//read the file content
	unsigned int bytesRead=1;
	int16_t i;
	uint8_t *para;
	bytesRead = 0;
	
	// TODO check: using this function for morph may be wrong
	if(isMorph)
		para=morphParameters;
	else
		para=mainParameters[presetNr];

	FRESULT res=f_read((FIL*)&SDFile, para,END_OF_SOUND_PARAMETERS, &bytesRead);
	if(res==FR_OK) {
		// set to 0 for any that were not read from the file
		if(END_OF_SOUND_PARAMETERS-bytesRead)
			memset(para+bytesRead,0,END_OF_SOUND_PARAMETERS-bytesRead);
		preset_readEngines(isMorph ? preset_morphEngines() : kitEngines_ram[presetNr]);
		
		//special case mod targets. normalize.
		const uint8_t nmt=getNumModTargets();
		for(i=0;i<6;i++) {
			// since I've changed the meaning of these, I'll ensure that it's valid for kits saved prior to the
			// change
			if(para[PAR_VEL_DEST_1+i] >= nmt )
				para[PAR_VEL_DEST_1+i] = 0;
			if(para[PAR_TARGET_LFO1+i] >= nmt )
				para[PAR_TARGET_LFO1+i] = 0;
		}
	}

	return res;

#endif
}

//----------------------------------------------------
FRESULT preset_loadDrumsetSD(uint8_t presetNr, uint8_t isMorph)
{
	//activePresetNr = presetNr;
#if USE_SD_CARD
	//filename in 8.3  format
	char filename[13];
	unsigned int bytesRead;

#if !CALC_SAMPLES_IN_INTERRUPT
	while(!soundengine_calcNextSampleBlock())
	{
		//wait till new block is calculated
	}
#endif

	//sprintf(filename,"p%03d.snd",presetNr);
	/*
	preset_makeFileNameLoad(filename,presetNr,FEXT_SOUND);

	//open the file
	FRESULT res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);
	*/
	FRESULT res = preset_makeFileNameLoad(filename,presetNr,FEXT_SOUND);
	if(res!=FR_OK)
	{
		//try old filename
		preset_makeFileNameOld(filename,presetNr,FEXT_SOUND);
		res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);
		if(res!=FR_OK)
			goto error; //file open error... maybe the file does not exist?
	}

#if !CALC_SAMPLES_IN_INTERRUPT
	//to prevent audio dropouts
	soundengine_calcNextSampleBlock();
#endif
	//first the preset name
	res=f_read((FIL*)&SDFile,(void*)savefileNames[presetNr][KIT_NAMES],8,&bytesRead);
	if(bytesRead != 8) {
		res=FR_DISK_ERR;
		goto closeFile;
	}
	if(preset_isNameEmpty(savefileNames[presetNr][KIT_NAMES]))
	{
		sprintf((void*)savefileNames[presetNr][KIT_NAMES], "Empty");
	}



#if !CALC_SAMPLES_IN_INTERRUPT
	//to prevent audio dropouts
	soundengine_calcNextSampleBlock();
#endif
	//then the data
	res=preset_readDrumsetData(presetNr, isMorph);
#if !CALC_SAMPLES_IN_INTERRUPT
	//to prevent audio dropouts
	soundengine_calcNextSampleBlock();
#endif
closeFile:
	//close the file handle
	f_close((FIL*)&SDFile);

	// update the soundengine with the new parameters
	if(res==FR_OK) {
		preset_sendDrumsetParameters();
		return FR_OK;
	}
error:
	return res;
#else
	frontPanel_sendData(PRESET,PRESET_LOAD,presetNr);
#endif
}
//----------------------------------------------------
//val is interpolation value between 0 and 255
//uses bankers rounding
static uint8_t interpolate(uint8_t a, uint8_t b, uint8_t x)
{
	uint16_t fixedPointValue = (uint16_t)(((a*256) + (b-a)*x));
	uint8_t result = (uint8_t)(fixedPointValue/256);

	return (uint8_t)((fixedPointValue&0xff) < 0x7f ? result : result+1);
	//return ((a*255) + (b-a)*x)/255;
}

//----------------------------------------------------
//kit load: not time critical, runs from flash (frees ITCM)
void preset_sendDrumsetParametersImmediate()
{
		//the kit's engines first: its parameters are for them
		kitEngines_apply();

		//special case mod targets
		for(uint8_t i=0;i<6;i++)
		{
			//**VELO load drumkit. translate to param value before sending
			// parameter_values[PAR_VEL_DEST_1+i] is an index into modTargets, we need to send
			// a parameter number
			uint8_t value = (uint8_t)pgm_read_word(&modTargets[activeParameters[PAR_VEL_DEST_1+i]].param);
			uint8_t upper,lower;
			upper = (uint8_t)(((value&0x80)>>7) | (((i)&0x3f)<<1));
			lower = value&0x7f;
			frontPanel_sendData(CC_VELO_TARGET,upper,lower);

			// ensure target voice # is valid
			if(activeParameters[PAR_VOICE_LFO1+i] < 1 || activeParameters[PAR_VOICE_LFO1+i] > 6 )
				activeParameters[PAR_VOICE_LFO1+i]=1;

			// **LFO par_target_lfo will be an index into modTargets, but we need a parameter number to send
			value = (uint8_t)pgm_read_word(&modTargets[activeParameters[PAR_TARGET_LFO1+i]].param);

			upper = (uint8_t)(((value&0x80)>>7) | (((i)&0x3f)<<1));
			lower = value&0x7f;
			frontPanel_sendData(CC_LFO_TARGET,upper,lower);
		}

		static MidiMsg msg = {0};
		//pre-send fx type to prevent clicks
		/*
		msg.status = CC_2;
		msg.data1 = PAR_FX_TYPE-128; //stupid offset only in CC not CC2
		msg.data2 = activeParameters[PAR_FX_TYPE];

		//	msg.data1 += 1;
		msg.data1 &= 0x7f;
		midiParser_ccHandler(msg,1, 0, 0.f);
		*/

		//try to prevent click by sending BD params first

		//stuff from the morph function
		for(uint32_t i=0;i<END_OF_SOUND_PARAMETERS; i++)
		{

			uint8_t paramNumber = parameterOrder[i];

			//const uint8_t morph = activeParameters[PAR_MORPH];

			const uint8_t val = activeParameters[paramNumber];//interpolate(activeParameters[i],morphParameters[i],morph);

			if(paramNumber < 128)
			{
				/* comes from cortex midi handler
				 * stupid offset!
				 * TODO fix this stupid offset			 *
				 * 		frontParser_midiMsg.data1 += 1;

						//because hh slope on front is 127 and on cortex is 0 wrap data1 at 127
						frontParser_midiMsg.data1 &= 0x7f;
				 *
				 */
				msg.status = MIDI_CC;
				msg.data1 = paramNumber;
				msg.data2 = val;

				msg.data1 += 1;
				msg.data1 &= 0x7f;
				midiParser_ccHandler(msg,1, 0, 0.f);


			}
			else
			{
				msg.status = CC_2;
				msg.data1 = paramNumber-128; //stupid offset only in CC not CC2
				msg.data2 = val;

			//	msg.data1 += 1;
				msg.data1 &= 0x7f;
				midiParser_ccHandler(msg,1, 0, 0.f);

			}
			soundengine_process();
		}
}
//----------------------------------------------------
// send loaded parameters to back
void preset_sendDrumsetParameters()
{
	uint8_t i;
	//the kit's engines first: its parameters are for them
	kitEngines_apply();
	//special case mod targets
	for(i=0;i<6;i++)
	{
		//**VELO load drumkit. translate to param value before sending
		// parameter_values[PAR_VEL_DEST_1+i] is an index into modTargets, we need to send
		// a parameter number
		uint8_t value = (uint8_t)pgm_read_word(&modTargets[activeParameters[PAR_VEL_DEST_1+i]].param);
		uint8_t upper,lower;
		upper = (uint8_t)(((value&0x80)>>7) | (((i)&0x3f)<<1));
		lower = value&0x7f;
		frontPanel_sendData(CC_VELO_TARGET,upper,lower);

		// ensure target voice # is valid
		if(activeParameters[PAR_VOICE_LFO1+i] < 1 || activeParameters[PAR_VOICE_LFO1+i] > 6 )
			activeParameters[PAR_VOICE_LFO1+i]=1;

		// **LFO par_target_lfo will be an index into modTargets, but we need a parameter number to send
		value = (uint8_t)pgm_read_word(&modTargets[activeParameters[PAR_TARGET_LFO1+i]].param);

		upper = (uint8_t)(((value&0x80)>>7) | (((i)&0x3f)<<1));
		lower = value&0x7f;
		frontPanel_sendData(CC_LFO_TARGET,upper,lower);
	}

	// todo will this morph (and fuck up) our modulation targets?
	// send parameters (possibly combined with morph parameters) to back
	preset_morph(activeParameters[PAR_MORPH]);



}
//----------------------------------------------------
/*
char* preset_loadName(uint8_t presetNr, uint8_t nameBufferIdx, uint8_t what)
{
	uint8_t type;

	switch(what) {
	case SAVE_TYPE_PATTERN:
		type=FEXT_PAT;
		break;
	case SAVE_TYPE_KIT:
	case SAVE_TYPE_MORPH:
		type=FEXT_SOUND;
		break;
	case SAVE_TYPE_ALL:
		type=FEXT_ALL;
		break;
	case SAVE_TYPE_PERFORMANCE:
		type=FEXT_PERF;
		break;
	default:
		return NULL; // for glo and sample we don't load a name
	}
	

#if USE_SD_CARD		
	//filename in 8.3  format
	char filename[9];
	preset_makeFileName(filename,presetNr,type);

	//try to open the file
	FRESULT res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);
	


	if(res!=FR_OK)
	{
		//error opening the file
		memcpy((void*)savefileNames[nameBufferIdx][presetNr],PSTR("Empty   "),8);
		return NULL;	
	}
	
	//file opened correctly -> extract name (first 8 bytes)
	unsigned int bytesRead;
	f_read((FIL*)&SDFile,(void*)savefileNames[nameBufferIdx][presetNr],8,&bytesRead);
	


	//close the file handle
	f_close((FIL*)&SDFile);
	
	return (char*)savefileNames[nameBufferIdx][presetNr];
#else

return "ToDo";
#endif
}
*/

//----------------------------------------------------
/** request step data from the cortex via uart and save it in the provided step struct*/
/*
void preset_queryStepDataFromSeq(uint16_t stepNr)
{
	frontParser_newSeqDataAvailable = 0;

	//request step data
	//the max number for 14 bit data is 16383!!!
	//current max step nr is 128*7*8 = 7168
	frontPanel_sendByte((stepNr>>7)&0x7f);		//upper nibble 7 bit
	frontPanel_sendByte(stepNr&0x7f);			//lower nibble 7 bit

	//wait until data arrives
	uint8_t newSeqDataLocal = 0;
	uint16_t now = systick_ticks;
	while((newSeqDataLocal==0))
	{
		//we have to call the uart parser to handle incoming messages from the sequencer
		uart_checkAndParse();
		
		newSeqDataLocal = frontParser_newSeqDataAvailable;
		
		if(systick_ticks-now >= 31)
		{
			//timeout
			now = systick_ticks;
			//request step again
			frontPanel_sendByte((stepNr>>7)&0x7f);		//upper nibble 7 bit
			frontPanel_sendByte(stepNr&0x7f);
		}
	}
}
*/
//----------------------------------------------------
/*
static void preset_sendMainStepDataToSeq( uint16_t mainStepData)
{
	frontPanel_sendByte(mainStepData	& 0x7f);
	frontPanel_sendByte((mainStepData>>7)	& 0x7f);
	frontPanel_sendByte((uint8_t)((mainStepData>>14)	& 0x7f));
}
*/
//----------------------------------------------------
/** send step data from SD card to the sequencer*/
/*
static void preset_sendStepDataToSeq()
{
	frontPanel_sendByte(frontParser_stepData.volume	& 0x7f);
	frontPanel_sendByte(frontParser_stepData.prob	& 0x7f);
	frontPanel_sendByte(frontParser_stepData.note	& 0x7f);

	frontPanel_sendByte(frontParser_stepData.param1Nr	& 0x7f);
	frontPanel_sendByte(frontParser_stepData.param1Val	& 0x7f);

	frontPanel_sendByte(frontParser_stepData.param2Nr	& 0x7f);
	frontPanel_sendByte(frontParser_stepData.param2Val	& 0x7f);

	//now the MSBs from all 7 values
	frontPanel_sendByte((uint8_t) 	(((frontParser_stepData.volume 	& 0x80)>>7) |
							((frontParser_stepData.prob	 	& 0x80)>>6) |
							((frontParser_stepData.note	 	& 0x80)>>5) |
							((frontParser_stepData.param1Nr	& 0x80)>>4) |
							((frontParser_stepData.param1Val	& 0x80)>>3) |
							((frontParser_stepData.param2Nr	& 0x80)>>2) |
							((frontParser_stepData.param2Val	& 0x80)>>1))
							);
}	
*/
//----------------------------------------------------
void preset_queryPatternInfoFromSeq(uint8_t patternNr, uint8_t* next, uint8_t* repeat)
{
	frontParser_newSeqDataAvailable = 0;
	//request pattern info
	frontPanel_sendByte(patternNr);	

	//wait until data arrives
	uint8_t newSeqDataLocal = 0;
	uint16_t now = systick_ticks;
	while((newSeqDataLocal==0))
	{
		//we have to call the uart parser to handle incoming messages from the sequencer
		uart_checkAndParse();
		
		newSeqDataLocal = frontParser_newSeqDataAvailable;
		
		if(systick_ticks-now >= 31)
		{
			//timeout
			now = systick_ticks;
			//request step again
			frontPanel_sendByte(patternNr);	
		}
	}
	
	//the stepdata struct is used as buffer for the data
	*next = frontParser_stepData.volume; 
	*repeat =  frontParser_stepData.prob;
}
//----------------------------------------------------
void preset_queryMainStepDataFromSeq(uint16_t stepNr, uint16_t *mainStepData, uint8_t *length)
{
	frontParser_newSeqDataAvailable = 0;

	//request step data
	//the max number for 14 bit data is 16383!!!
	//current max step nr is 7*8 = 56
	frontPanel_sendByte((stepNr>>7)&0x7f);		//upper nibble 7 bit
	frontPanel_sendByte(stepNr&0x7f);	//lower nibble 7 bit

	//wait until data arrives
	uint8_t newSeqDataLocal = 0;
	uint16_t now = systick_ticks;
	while((newSeqDataLocal==0))
	{
		//we have to call the uart parser to handle incoming messages from the sequencer
		uart_checkAndParse();
		
		newSeqDataLocal = frontParser_newSeqDataAvailable;
		
		if(systick_ticks-now >= 31)
		{
			//timeout
			now = systick_ticks;
			//request step again
			frontPanel_sendByte((stepNr>>7)&0x7f);		//upper nibble 7 bit
			frontPanel_sendByte(stepNr&0x7f);
		}
	}
	
	// we are reusing these members for purposes other than those that were originally intended
	*mainStepData =(uint16_t) ((frontParser_stepData.volume<<8) | frontParser_stepData.prob);
	*length=frontParser_stepData.note;
};

//----------------------------------------------------
static void preset_writePatternData(uint8_t patternNr)
{
#if 0
#else
		UINT bytesWritten;
		uint8_t length;

		//---------------  write the step data (note, vol, prob, autom.) ---------------
		for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
		{
			preset_fwrite((FIL*)&SDFile,(const void*)&seq_patternSet.seq_subStepPattern[patternNr][trk][0],sizeof(Step)*STEPS_PER_PATTERN,&bytesWritten);

			preset_fwrite((FIL*)&SDFile,(const void*)&seq_patternSet.seq_stepTiming[patternNr][trk][0],sizeof(uint8_t)*STEPS_PER_PATTERN,&bytesWritten);
		}

		//--------------- now the main step data ---------------
		for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
		{
			for(uint8_t bar=0;bar<NUM_BARS; bar++)
			{
				const uint16_t mainStepData = seq_patternSet.seq_mainSteps[bar][patternNr][trk];
				preset_fwrite((FIL*)&SDFile,(const void*)&mainStepData,sizeof(uint16_t),&bytesWritten);
			}
		}

		//----- shuffle setting ------


		//preset_fwrite((FIL*)&SDFile,(const void*)&activeParameters[PAR_SHUFFLE],sizeof(uint8_t),&bytesWritten);
		preset_fwrite((FIL*)&SDFile,(const void*)&seq_patternSet.shuffle[patternNr],sizeof(uint8_t),&bytesWritten);

		//----- pattern/track lengths ------
		// we reuse the same call from above (when saving main step data)
		// but we only use the length info retrieved. We want to store it at the end
		// to avoid breaking compatibility with save file


		for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
		{
				length = seq_patternSet.length[patternNr][trk] ;//seq_patternSet.seq_mainSteps[bar][pat][trk];
				preset_fwrite((FIL*)&SDFile,(const void*)&length,sizeof(uint8_t),&bytesWritten);
		}

		//write associated kit nr
		preset_fwrite((FIL*)&SDFile,&seq_patternSet.kitNr[patternNr],sizeof(uint8_t),&bytesWritten);

#endif
}

//----------------------------------------------------
/*
void preset_savePatternTemp(uint8_t presetNr)
{

	UINT bytesWritten;
	//lcd_clear();
	//lcd_home();
	//lcd_string(PSTR("Saving pattern"));


	//filename in 8.3  format
	char filename[9];
	//sprintf(filename,"p%03d.pat",presetNr);
	preset_makeFileNameTemp(filename,presetNr,FEXT_PAT);
	//open the file
	f_open((FIL*)&SDFile,filename,FA_CREATE_ALWAYS | FA_WRITE);

	//write preset name to file
	preset_fwrite((FIL*)&SDFile,(void*)preset_currentName,8,&bytesWritten);

	preset_writePatternData();

	//close the file
	f_close((FIL*)&SDFile);

	//reset the lcd
	menu_repaintAll();


}
*/
//----------------------------------------------------
FRESULT preset_savePatternSD(uint8_t presetNr)
{

	FRESULT res;
	UINT bytesWritten;
	//lcd_clear();
	//lcd_home();
	//lcd_string(PSTR("Saving pattern"));
	
	
	//filename in 8.3  format
	char filename[13];
	//sprintf(filename,"p%03d.pat",presetNr);
	preset_makeFileNameSave(filename,presetNr,FEXT_PAT, savefileNames[presetNr][PATTERN_NAMES]);
	//open the file
	res = preset_fopenWrite(filename);
	if(res != FR_OK) return res;

	//write preset name to file (after an error the writes are skipped, the close reports it)
	preset_fwrite((FIL*)&SDFile,(void*)savefileNames[presetNr][PATTERN_NAMES],8,&bytesWritten);
	//write version tag
	preset_fwrite((FIL*)&SDFile,"PatV03",6,&bytesWritten);

	//write pattern data
	preset_writePatternData(presetNr);

	//close the file
	res = preset_fcloseWrite();
	if(res == FR_OK)
		res = preset_removeOtherFiles(presetNr, FEXT_PAT, filename);
	
	//reset the lcd
	menu_repaintAll();
	
	return res;
}

//----------------------------------------------------
// returns 1 on success
static uint8_t preset_readPatternData(uint8_t patternNr, uint8_t dataVersion)
{

#if 0


	// note that the pattern length data is no longer stored in the same way.
	// This means that loading old patterns with non-standard length, the length will be
	// ignored and a standard length used. I think it's acceptable as long as users are aware of this.

	UINT bytesRead;
	uint8_t success=1; // start off succeeding
	//uint16_t i;
	//uint8_t repeat=0;
	uint8_t next=0;

	//lcd_clear();
	//lcd_home();
	//lcd_string(PSTR("Loading pattern"));

	// ---------- Load step data first



	// load step data


	for(uint8_t pat=0; pat<NUM_PATTERN;pat++)
	{
		for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
		{

#if 1

			if( (pat == seq_activePattern) && seq_isRunning() )
			{
				f_read((FIL*)&SDFile,(void*)&seq_tmpPattern.seq_subStepPattern[trk][0],sizeof(Step)*STEPS_PER_PATTERN,&bytesRead);
			} else {
				f_read((FIL*)&SDFile,(void*)&seq_patternSet.seq_subStepPattern[pat][trk][0],sizeof(Step)*STEPS_PER_PATTERN,&bytesRead);
			}



			if(bytesRead!=sizeof(Step)*(STEPS_PER_PATTERN))
			{
				success=0;
				break; // file error, or we ran out of file (eof)
			}
#else
			for(uint8_t stp=0;stp<STEPS_PER_PATTERN; stp++)
			{
				f_read((FIL*)&SDFile,(void*)&frontParser_stepData,sizeof(Step),&bytesRead);
				if(bytesRead==sizeof(Step))
				{
					seq_patternSet.seq_subStepPattern[pat][trk][stp] = frontParser_stepData;
				} else {
					success=0;
					break; // file error, or we ran out of file (eof)
				}
			}
#endif
		}
	}

	// ---------- load the main step data next
	uint16_t mainStepData;

	for(uint8_t pat=0; pat<NUM_PATTERN;pat++)
	{
		for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
		{
			for(uint8_t bar=0;bar<NUM_BARS; bar++)
			{
				f_read((FIL*)&SDFile,(void*)&mainStepData,sizeof(uint16_t),&bytesRead);

				if( bytesRead==sizeof(uint16_t)) {

					if( (pat == seq_activePattern) && seq_isRunning() )
					{
						seq_tmpPattern.seq_mainSteps[bar][trk] = mainStepData;
					} else {
						seq_patternSet.seq_mainSteps[bar][pat][trk] = mainStepData;
					}
				}
				 else {
						success=0;
						break;
					}
			}
		}
	}

	//---------- pattern info (next/repeat)
	/*
	if(success) {
		for(i=0;i<(NUM_PATTERN);i++)
		{
			f_read((FIL*)&preset_File,(void*)&next,sizeof(uint8_t),&bytesRead);
			if(!bytesRead) {
				success=0;
				break;
			}
			f_read((FIL*)&preset_File,(void*)&repeat,sizeof(uint8_t),&bytesRead);
			if(!bytesRead) {
				success=0;
				break;
			}

			if( (i == seq_activePattern) && seq_isRunning() )
			{
				seq_tmpPattern.seq_patternSettings.nextPattern = next;
				seq_tmpPattern.seq_patternSettings.changeBar = repeat;
			} else {
				seq_patternSet.seq_patternSettings[i].nextPattern = next;
				seq_patternSet.seq_patternSettings[i].changeBar = repeat;
			}

		}
	}
	*/

	// ------------ shuffle settings
	if(success) {
		//load the shuffle settings
		f_read((FIL*)&SDFile,(void*)&activeParameters[PAR_SHUFFLE],sizeof(uint8_t),&bytesRead);
		//and update on cortex
		if(bytesRead)
			//frontPanel_sendData(SEQ_CC,SEQ_SHUFFLE,parameter_values[PAR_SHUFFLE]);
			seq_setShuffle(activeParameters[PAR_SHUFFLE]/127.f);
		else {
			success=0;
		}
	}

	// ----------- Pattern/track lengths
	// -- AS this might not exist in the saved data file, we still want success
	if(success) {


		for(uint8_t pat=0; pat<NUM_PATTERN;pat++)
		{
			for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
			{
				f_read((FIL*)&SDFile,(void*)&next,sizeof(uint8_t),&bytesRead);


				if( (pat == seq_activePattern) && seq_isRunning() )
				{
					seq_tmpPattern.length[trk] = preset_validLength(next);
				} else {
					seq_patternSet.length[pat][trk] = preset_validLength(next);
				}
			}
		}

		// signal new pattern after receiving all the data
		/*
		if( seq_isRunning() ) {
			seq_newPatternAvailable = 1;
		}
		*/

		success=1; // just to document that we don't consider this a failure, we just didn't read the bytes for this
	}

	return success;
#else
	// note that the pattern length data is no longer stored in the same way.
	// This means that loading old patterns with non-standard length, the length will be
	// ignored and a standard length used. I think it's acceptable as long as users are aware of this.
	//everything is read into preset_patternBuffer first and only copied into the pattern after the
	//whole file was read: a damaged file leaves the pattern as it was
	PatternFileData* buf = &preset_patternBuffer;
	UINT bytesRead;
	uint8_t next=0;
	// ---------- Load step data first
	// load step data
	for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
	{
		f_read((FIL*)&SDFile,(void*)&buf->steps[trk][0],sizeof(Step)*STEPS_PER_PATTERN,&bytesRead);
		if(bytesRead!=sizeof(Step)*(STEPS_PER_PATTERN))
			return 0; // file error, or we ran out of file (eof)
		//Pattern V3 and up => flam added
		if(dataVersion > 2)
		{
			f_read((FIL*)&SDFile,(void*)&buf->timing[trk][0],sizeof(StepTiming)*STEPS_PER_PATTERN,&bytesRead);
			if(bytesRead!=sizeof(StepTiming)*(STEPS_PER_PATTERN))
				return 0; // file error, or we ran out of file (eof)
		}
		else
		{
			//older files: no flam, no shift
			for(uint8_t stp=0; stp<STEPS_PER_PATTERN; stp++)
			{
				buf->timing[trk][stp].flam = 0;
				buf->timing[trk][stp].shift = 7;   // 7 = no shift
			}
		}
	}
	// ---------- load the main step data next
	for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
	{
		for(uint8_t bar=0;bar<NUM_BARS; bar++)
		{
			f_read((FIL*)&SDFile,(void*)&buf->mainSteps[bar][trk],sizeof(uint16_t),&bytesRead);
			if( bytesRead!=sizeof(uint16_t))
				return 0;
		}
	}
	// ------------ shuffle settings
	f_read((FIL*)&SDFile,(void*)&buf->shuffle,sizeof(uint8_t),&bytesRead);
	if(!bytesRead)
		return 0;
	// ----------- Pattern/track lengths
	// -- AS this might not exist in the saved data file, we still want success
	for(uint8_t trk=0; trk<NUM_TRACKS;trk++)
	{
		f_read((FIL*)&SDFile,(void*)&next,sizeof(uint8_t),&bytesRead);
		buf->length[trk] = preset_validLength(next);
	}
	//V1 files have no kit nr: the pattern keeps its kit nr
	buf->kitNr = seq_patternSet.kitNr[patternNr];
	if(dataVersion > 1)
	{
		f_read((FIL*)&SDFile,(void*)&buf->kitNr,sizeof(uint8_t),&bytesRead);
		if(bytesRead == 0)
			return 0;
	}

	//complete: copy into the pattern
	memcpy(seq_patternSet.seq_subStepPattern[patternNr], buf->steps, sizeof(buf->steps));
	memcpy(seq_patternSet.seq_stepTiming[patternNr], buf->timing, sizeof(buf->timing));
	for(uint8_t bar=0;bar<NUM_BARS; bar++)
		memcpy(seq_patternSet.seq_mainSteps[bar][patternNr], buf->mainSteps[bar], sizeof(buf->mainSteps[bar]));
	memcpy(seq_patternSet.length[patternNr], buf->length, sizeof(buf->length));
	seq_patternSet.shuffle[patternNr] = buf->shuffle;
	seq_patternSet.kitNr[patternNr] = buf->kitNr;
	preset_validateKitNr(patternNr);
	return 1;
#endif
}
//----------------------------------------------------
/*
uint8_t preset_loadPatternTemp(uint8_t presetNr)
{
#if USE_SD_CARD
	//filename in 8.3  format
	char filename[9];
	UINT bytesRead;
	uint8_t success=0;
	//sprintf(filename,"p%03d.pat",presetNr);
	preset_makeFileNameTemp(filename,presetNr,FEXT_PAT);

	//open the file
	FRESULT res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);

	if(res!=FR_OK)
	{

		return 0; //file open error... maybe the file does not exist?
	}


	//read the preset name
	f_read((FIL*)&SDFile,(void*)preset_currentName,8,&bytesRead);
	if(bytesRead != 8)
		goto closeFile;

	if(!preset_readPatternData())
		goto closeFile;

	success=1;
	//close the file handle
closeFile:
	f_close((FIL*)&SDFile);

	//force complete repaint
	menu_repaintAll();
	// return success or failure
	return success;

#else
frontPanel_sendData(PRESET,PATTERN_LOAD,presetNr);

#endif
}
*/
//----------------------------------------------------
FRESULT preset_loadPatternSD(uint8_t presetNr)
{
#if USE_SD_CARD		
	//filename in 8.3  format
	uint8_t fileVersion = 1;
	char filename[13];
	UINT bytesRead;
	uint8_t success=0;
	//sprintf(filename,"p%03d.pat",presetNr);

	/*
	preset_makeFileNameLoad(filename,presetNr,FEXT_PAT);

	//open the file
	FRESULT res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);
	*/
	FRESULT res = preset_makeFileNameLoad(filename,presetNr,FEXT_PAT);
	if(res!=FR_OK)
	{
		//try old filename
		preset_makeFileNameOld(filename,presetNr,FEXT_PAT);
		res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);
		if(res!=FR_OK)
			return res; //file open error... maybe the file does not exist?
	}
	
	//read the preset name
	res = f_read((FIL*)&SDFile,(void*)savefileNames[presetNr][PATTERN_NAMES],8,&bytesRead);
	if(bytesRead != 8)
		goto closeFile;
	
	char buf[7];
	res = f_read((FIL*)&SDFile,buf,6,&bytesRead);
	//zero termination
	buf[6]=0;
	fileVersion = atoi(&buf[4]);
	buf[4] = 0; // PatV\0

	if(strcmp(buf, "PatV") == 0)
	{
		//we have a new version pattern preset >V0 (V0 has no version tag)


	}
	else
	{
		//V1 preset data without version tag -> rewind read pointer
		fileVersion = 1; //re-assign since atoi() broke the 1
		f_lseek((FIL*)&SDFile, f_tell((FIL*)&SDFile) - 6);
	}
	//preset_fwrite((FIL*)&SDFile,"PatV02",6,&bytesWritten);

	if(!preset_readPatternData(presetNr, fileVersion))
	{
		goto closeFile;
	}
	
	success=1;
	//close the file handle



closeFile:
	f_close((FIL*)&SDFile);
	
	//force complete repaint
	menu_repaintAll();
	// return success or failure
	if(success) return FR_OK;
	else return FR_INT_ERR;
	
#else
frontPanel_sendData(PRESET,PATTERN_LOAD,presetNr);

#endif
}

//----------------------------------------------------
FRESULT preset_savePatternRAM(uint8_t presetNr)
{

	//check if current slot or new slot
	if(presetNr == seq_activePattern)
	{
		//### Current Slot ###
		//write current (changed) pattern to backup buffer, to make changes permanent
		seq_copyPatternToBackupBuffer(presetNr);
	}
	else
	{
		//### NEW SLOT ###
		//- write current (changed) pattern to new slot
		seq_copyPattern(seq_activePattern, presetNr);
		//- change active pattern to new slot (writes backup buffer back to current slot, reverting changes being made in current slot)
		seq_setNextPattern(presetNr);
	}

	return FR_OK;
}
//----------------------------------------------------
FRESULT preset_loadPatternRAM(uint8_t presetNr)
{

	//I'm unused since there is no Pattern Load page, it just uses the pattern change/select page instead
	return FR_OK;
}
//----------------------------------------------------
//called periodically to process non blocking morph
void preset_morph_tick()
{
	//morph_state = 0;
	//return;
	if(morph_state)
	{

		uint8_t morph;

		//Do not morph audio routing
		if((morph_index >= PAR_AUDIO_OUT1) && (morph_index <= PAR_AUDIO_OUT6)) //morph_index++;
		{
			//always use source kit (morph = 0) for audio routing
			morph = 0;
		}
		else if(preset_isMorphEngineMismatch(morph_index))
		{
			//engine specific parameter of a slot whose engine differs in the morph
			//kit: the values mean different things, use the source kit's
			morph = 0;
		}
		else
		{
			morph = activeParameters[PAR_MORPH];
		}

		static MidiMsg msg = {0};


		const uint8_t val = interpolate(activeParameters[morph_index],morphParameters[morph_index],morph);


		if(morph_index < 128)
		{
			/* comes from cortex midi handler
			 * stupid offset!
			 * TODO fix this stupid offset			 *
			 * 		frontParser_midiMsg.data1 += 1;

					//because hh slope on front is 127 and on cortex is 0 wrap data1 at 127
					frontParser_midiMsg.data1 &= 0x7f;
			 *
			 */
			msg.status = MIDI_CC;
			msg.data1 = morph_index;
			msg.data2 = val;

			msg.data1 += 1;
			msg.data1 &= 0x7f;
			midiParser_ccHandler(msg,1, 0, 0.f);


		}
		else
		{
			msg.status = CC_2;
			msg.data1 = morph_index-128; //stupid offset only in CC not CC2
			msg.data2 = val;

		//	msg.data1 += 1;
			msg.data1 &= 0x7f;
			midiParser_ccHandler(msg,1, 0, 0.f);

		}


		morph_index++;
		if(morph_index >= END_OF_SOUND_PARAMETERS)
		{
			//finished morphing
			morph_state = 0;
		}
	}
}
//----------------------------------------------------
// This will determine an interpolated value between parameters
// and morph parameters and send the data to the back
// if the morph value is 0 it will just send the parameter values
// to the back
void preset_morph(uint8_t morph)
{
#if 1
	//initialize new morph
	morph_state = 1; //start morph
	morph_index = 0; //reset index
#else
	int i;
	//TODO so far only sound parameters are interpolated, no LFOs etc

	for(i=0;i<END_OF_SOUND_PARAMETERS;i++)
	{


		uint8_t val;
		
		val = interpolate(mainParameters[i],morphParameters[i],morph);

		if(i<128) {
			frontPanel_sendData(MIDI_CC,(uint8_t)i,val);

		} else {
			frontPanel_sendData(CC_2,(uint8_t)(i-128),val);

		}		
		
			
		//to omit front panel button/LED lag we have to process din dout and uart here
		//read next button

		//update LEDs
		dio_process();
		//read uart messages from sequencer
		uart_checkAndParse();
	}		
#endif
}

//----------------------------------------------------
uint8_t preset_getMorphValue(uint16_t index, uint8_t morph)
{
	if(preset_isMorphEngineMismatch(index))
		return activeParameters[index];
	return interpolate(activeParameters[index],morphParameters[index],morph);
};
//----------------------------------------------------

FRESULT preset_createProject(uint8_t presetNr)
{


	//create default globals

	//clear all patterns

	//clear all kits

	//clear all songs

	//load init kit


	int i;

	preset_loadDefaultGlobals();
	//give active song pointer a proper value
	seq_activeSong = seq_songData[0];

	memset(seq_songData, SONG_END, sizeof(uint8_t) * MAX_SONG_LENGTH * NUM_KIT_SLOTS);
	seq_songData[0][0] = 0;


	//memset(seq_stepIndex,0,NUM_TRACKS);
//	memset(seq_lastMasterStep,0,NUM_TRACKS);


	for(i=0;i<NUM_PATTERN;i++)
	{
		//seq_patternSet.seq_patternSettings[i].changeBar 	= 0;	//default setting: zero repeats (play once then change)
		//seq_patternSet.seq_patternSettings[i].nextPattern 	= i;	//default setting: repeat same pattern
		seq_clearPattern(i); // will clear all tracks in the pattern
		preset_init(i); //set all kits to init sound
	}

	sprintf(savefileNames[presetNr][PROJECT_NAMES], "Init");

	preset_clearTmpFolder();

	return FR_OK;
}
//----------------------------------------------------
// **SAVEALL
// save all settings (global, pattern, kit)
// or save performance settings (pattern, kit)

FRESULT preset_saveProject(uint8_t presetNr)
{
	FRESULT res;
	FILINFO fno;

	//the current kit (not the one last scrolled to on the load/save page)
	preset_saveDrumsetRam(menu_loadedPresetNr[CURRENT_KIT], 0);
	preset_savePatternRAM(seq_activePattern);

#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
	HAL_TIM_Base_Start_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif

	lcd_process_display_interrupt = 0;
	while(sendDisplayBuffer() != 0);

	lcd_clear();
	lcd_home();
	lcd_string(PSTR("Saving Project"));
	lcd_setcursor(0,2);
	lcd_string(PSTR("0%"));
	/*
	MX_FATFS_ReInit();
	f_mount(0, (TCHAR const*)SDPath, 0); //un-mount
	memset((FATFS*)&SDFatFS,0,sizeof(FATFS)); //clear FS object to force reinit
	res = f_mount((FATFS*)&SDFatFS, (TCHAR const*)SDPath,1);
	if( res != FR_OK)
	{
		return res;
	}
*/
	//write into a new folder and replace the project folder only when everything was written:
	//a failed save (full or bad card) leaves the saved project as it was
	char newFolder[12];
	char oldFolder[12];
	preset_projectFolderName(currentFolder, presetNr);
	sprintf(newFolder, "/proj%02d" PROJ_NEW_SUFFIX, presetNr);
	sprintf(oldFolder, "/proj%02d" PROJ_OLD_SUFFIX, presetNr);

	//leftovers of an interrupted save
	f_chdir("/");
	res = preset_removeFolder(newFolder);
	if(res == FR_OK)
		res = preset_removeFolder(oldFolder);
	//a new project gets an empty folder first: /projNN is only missing between the renames below
	if(res == FR_OK && f_stat(currentFolder, &fno) == FR_NO_FILE)
		res = f_mkdir(currentFolder);
	if(res == FR_OK)
		res = f_mkdir(newFolder);
	if(res == FR_OK)
		res = f_chdir(newFolder);
	if(res != FR_OK)
		goto saveFailed;
	preset_writingNewFolder = 1;

	//save data

#if 1



	char text[17];

	//Save Project name
	//open the file
	res = preset_fopenWrite("prj.nfo");
	if(res != FR_OK) {
		goto saveFailed;
	}
	unsigned int bytesWritten;
	//write preset name to file
	preset_fwrite((FIL*)&SDFile,(void*)savefileNames[presetNr][PROJECT_NAMES],8,&bytesWritten);
	res = preset_fcloseWrite();
	if(res != FR_OK) {
		goto saveFailed;
	}

//	soundengine_process();
	//save globals
	res = preset_saveGlobals();
	if(res != FR_OK) {
		goto saveFailed;
	}
	//save all drumkits, patterns and songs
	for(uint8_t i=0;i<NUM_KIT_SLOTS;i++)
	{
		sprintf(text, "%d%%", (uint8_t)(i/(float)NUM_KIT_SLOTS * 100.f));
		lcd_setcursor(0,2);
		lcd_string(text);
	//	soundengine_process();
		res = preset_saveDrumsetSD(i, 0);
		if(res != FR_OK)
			goto saveFailed;
	//	soundengine_process();
		res = preset_savePatternSD(i);
		if(res != FR_OK)
			goto saveFailed;
	//	soundengine_process();
		res = preset_saveSongSD(i);
		if(res != FR_OK)
			goto saveFailed;
	}

	preset_writingNewFolder = 0;

	//complete: replace the project folder (power off in between: preset_projectFolderName() finishes it)
	f_chdir("/");
	res = f_rename(currentFolder, oldFolder);
	if(res != FR_OK)
		goto saveFailed;
	res = f_rename(newFolder, currentFolder);
	if(res != FR_OK)
	{
		f_rename(oldFolder, currentFolder);
		goto saveFailed;
	}
	//the save is done: if this fails, the next save removes the old folder
	preset_removeFolder(oldFolder);

	//(also changes into /tmp) still with the audio interrupt running: the main loop doesn't
	//render audio until this function returns (was after stopping it: audio dropouts)
	preset_clearTmpFolder();

	lcd_process_display_interrupt = 1;
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
	HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif

	return FR_OK;

saveFailed:
	preset_writingNewFolder = 0;
	f_close((FIL*)&SDFile);
	f_chdir("/");
	preset_removeFolder(newFolder);
	lcd_process_display_interrupt = 1;
	preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
	HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
	return res;

#else
#if USE_SD_CARD
	uint16_t remain;
	uint8_t siz;
	//filename in 8.3  format
	char filename[GEN_BUF_LEN];
	if(isAll)
		preset_makeFileName(filename,presetNr,FEXT_ALL);
	else
		preset_makeFileName(filename,presetNr,FEXT_PROJ);
	//open the file
	f_open((FIL*)&SDFile,filename,FA_CREATE_ALWAYS | FA_WRITE);


	unsigned int bytesWritten;
	//write preset name to file
	preset_fwrite((FIL*)&SDFile,(void*)savefileNames[KIT_NAMES][presetNr],8,&bytesWritten);

	//write the version
	filename[0]=FILE_VERSION;
	preset_fwrite((FIL*)&SDFile, filename, 1, &bytesWritten);

	memset(filename,0xFF,GEN_BUF_LEN); // reuse this as our buffer for padding

	if(isAll) {
		// write global settings
		preset_writeGlobalData();

		// write some padding (we'll allocate 64 bytes for globals, right now we have +/- 18) so that
		// we don't need to change the file format later
		remain=	64- (NUM_PARAMS - PAR_BEGINNING_OF_GLOBALS);

	} else {
		// write the bpm
		preset_fwrite((FIL*)&SDFile, &activeParameters[PAR_BPM], 1, &bytesWritten);
		// bar reset mode
		preset_fwrite((FIL*)&SDFile, &activeParameters[PAR_BAR_RESET_MODE], 1, &bytesWritten);

		remain=	64 - (1+1);
	}

	// write padding after globals/perf data
	while(remain){
		if(remain < GEN_BUF_LEN)
			siz=(uint8_t)remain;
		else
			siz=GEN_BUF_LEN;
		preset_fwrite((FIL*)&SDFile, filename, siz, &bytesWritten);
		remain -=siz;
	}

	// save kit
	preset_writeDrumsetData(0);

	// write some more padding (we'll allocate 512 bytes for kit data. This should be more than
	// we'll ever need. Right now we use about 228 bytes).
	remain = 512 - (END_OF_SOUND_PARAMETERS);
	while(remain){
		if(remain < GEN_BUF_LEN)
			siz=(uint8_t)remain;
		else
			siz=GEN_BUF_LEN;
		preset_fwrite((FIL*)&SDFile, filename, siz, &bytesWritten);
		remain -=siz;
	}

	// write pattern data
	preset_writePatternData(0);

	//close the file
	f_close((FIL*)&SDFile);
#else
#endif
#endif
}

//----------------------------------------------------
FRESULT preset_loadSongSD(uint8_t songNr)
{
	FRESULT res;
	char filename[13];
	UINT bytesRead;

	/*
	preset_makeFileNameLoad(filename,songNr,FEXT_SONG);
	//open the file
	res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);
	*/
	res = preset_makeFileNameLoad(filename,songNr,FEXT_SONG);

	if(res!=FR_OK)
	{
		//try old filename
		preset_makeFileNameOld(filename,songNr,FEXT_SONG);
		res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);
		if(res!=FR_OK)
			return res; //file open error... maybe the file does not exist?
	}

	//first the song name
	res = f_read((FIL*)&SDFile,(void*)savefileNames[songNr][SONG_NAMES],8,&bytesRead);

	if((!bytesRead) || (res != FR_OK))
		goto closeFile;

	if(preset_isNameEmpty(savefileNames[songNr][SONG_NAMES]))
	{
		sprintf((void*)savefileNames[songNr][SONG_NAMES], "Empty");
	}

	//read song data
	res = f_read((FIL*)&SDFile, seq_songData[songNr], MAX_SONG_LENGTH, &bytesRead);

	closeFile:
		//close the file handle
		f_close((FIL*)&SDFile);
		return res;
}
//----------------------------------------------------
FRESULT preset_saveSongSD(uint8_t songNr)
{
	FRESULT res;
	char filename[13];
	unsigned int bytesWritten;
	preset_makeFileNameSave(filename,songNr,FEXT_SONG, savefileNames[songNr][SONG_NAMES]);
	//open the file
	res = preset_fopenWrite(filename);
	if(res!=FR_OK)
		return res; //file open error... maybe the file does not exist?
	//write name
	res = preset_fwrite((FIL*)&SDFile,(void*)savefileNames[songNr][SONG_NAMES],8,&bytesWritten);
	if (res != FR_OK)
	{
		f_close((FIL*)&SDFile);
		return res;
	}

	//write song data
	res = preset_fwrite((FIL*)&SDFile,seq_songData[songNr],MAX_SONG_LENGTH,&bytesWritten);
	if (res != FR_OK)
	{
		f_close((FIL*)&SDFile);
		return res;
	}

	//close the file handle
	res = preset_fcloseWrite();
	if(res == FR_OK)
		res = preset_removeOtherFiles(songNr, FEXT_SONG, filename);
	return res;
}


//----------------------------------------------------
//----------------------------------------------------
//index of a kit file in a project folder: NN-NAME.SND (0-63) or the older
//PNNN.SND (isOld = 1); -1 for any other file
static int8_t preset_kitFileIndex(const char* fname, uint8_t* isOld)
{
	const char* dot = strchr(fname, '.');
	if(!dot || strcasecmp(dot, ".snd") != 0)
		return -1;
	if(fname[0] >= '0' && fname[0] <= '9' && fname[1] >= '0' && fname[1] <= '9' && fname[2] == '-')
	{
		*isOld = 0;
		const int8_t nr = (int8_t)((fname[0]-'0')*10 + (fname[1]-'0'));
		return nr < NUM_KIT_SLOTS ? nr : -1;
	}
	if((fname[0] == 'p' || fname[0] == 'P') && dot - fname == 4)
	{
		*isOld = 1;
		const int nr = atoi(&fname[1]);
		return nr < NUM_KIT_SLOTS ? (int8_t)nr : -1;
	}
	return -1;
}
//----------------------------------------------------
//find the file of a kit in a project folder (the NN- name wins over the old one)
static FRESULT preset_findProjectKit(const char* folder, uint8_t kitNr, char* path)
{
	DIR dir;
	FILINFO fno;
	uint8_t found = 0;
	FRESULT res = f_opendir(&dir, folder);
	if(res != FR_OK)
		return res;
	while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
	{
		uint8_t isOld;
		if(preset_kitFileIndex(fno.fname, &isOld) == kitNr && (!found || !isOld))
		{
			sprintf(path, "%s/%s", folder, fno.fname);
			found = isOld ? 1 : 2;
			if(found == 2)
				break;
		}
	}
	f_closedir(&dir);
	return found ? FR_OK : FR_NO_FILE;
}
//----------------------------------------------------
FRESULT preset_scanProjectKits(uint8_t projectNr, char names[][8], uint8_t engines[][6], uint8_t* exists)
{
	//FatFs allows one open file or folder at a time (_FS_LOCK 1): first collect
	//the kit files, then read them
	static char kitFiles[NUM_KIT_SLOTS][13];
	uint8_t isOldFile[NUM_KIT_SLOTS];
	char folder[12];
	char path[32];
	DIR dir;
	FILINFO fno;

	memset(exists, 0, NUM_KIT_SLOTS);
	preset_projectFolderName(folder, projectNr);
	FRESULT res = f_opendir(&dir, folder);
	if(res != FR_OK)
		return res;
	while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
	{
		uint8_t isOld;
		const int8_t kit = preset_kitFileIndex(fno.fname, &isOld);
		//the NN- name wins over the old one
		if(kit < 0 || (exists[kit] && (isOld || !isOldFile[kit])))
			continue;
		strcpy(kitFiles[kit], fno.fname);
		exists[kit] = 1;
		isOldFile[kit] = isOld;
	}
	f_closedir(&dir);

	for(uint8_t kit=0;kit<NUM_KIT_SLOTS;kit++)
	{
		if(!exists[kit])
			continue;
		exists[kit] = 0;
		snprintf(path, sizeof(path), "%s/%.12s", folder, kitFiles[kit]);
		if(f_open((FIL*)&SDFile, path, FA_OPEN_EXISTING | FA_READ) != FR_OK)
			continue;
		UINT bytesRead;
		if(f_read((FIL*)&SDFile, names[kit], 8, &bytesRead) == FR_OK && bytesRead == 8
				&& f_lseek((FIL*)&SDFile, 8 + END_OF_SOUND_PARAMETERS) == FR_OK)
		{
			preset_readEngines(engines[kit]);
			exists[kit] = 1;
		}
		f_close((FIL*)&SDFile);
	}
	return FR_OK;
}
//----------------------------------------------------
FRESULT preset_readProjectKit(uint8_t projectNr, uint8_t kitNr, uint8_t* params, uint8_t* engines)
{
	char folder[12];
	char path[32];
	preset_projectFolderName(folder, projectNr);
	FRESULT res = preset_findProjectKit(folder, kitNr, path);
	if(res != FR_OK)
		return res;
	res = f_open((FIL*)&SDFile, path, FA_OPEN_EXISTING | FA_READ);
	if(res != FR_OK)
		return res;

	UINT bytesRead = 0;
	res = f_lseek((FIL*)&SDFile, 8);	//skip the name
	if(res == FR_OK)
		res = f_read((FIL*)&SDFile, params, END_OF_SOUND_PARAMETERS, &bytesRead);
	if(res == FR_OK)
	{
		//like loading a kit: missing parameters 0, mod targets normalized
		memset(params + bytesRead, 0, END_OF_SOUND_PARAMETERS - bytesRead);
		const uint8_t nmt = getNumModTargets();
		for(uint8_t i=0;i<6;i++)
		{
			if(params[PAR_VEL_DEST_1+i] >= nmt)
				params[PAR_VEL_DEST_1+i] = 0;
			if(params[PAR_TARGET_LFO1+i] >= nmt)
				params[PAR_TARGET_LFO1+i] = 0;
		}
		preset_readEngines(engines);
	}
	f_close((FIL*)&SDFile);
	return res;
}
//----------------------------------------------------
FRESULT preset_saveProjectKit(uint8_t projectNr, uint8_t kitNr, char* name, const uint8_t* params, const uint8_t* engines)
{
	char folder[12];
	char path[32];
	char fileName[13];
	FILINFO fno;
	UINT bytesWritten;

	preset_projectFolderName(folder, projectNr);
	FRESULT res = f_stat(folder, &fno);
	if(res != FR_OK)
		return res;

	//the file name contains the kit name: remove the kit's old file first
	while(preset_findProjectKit(folder, kitNr, path) == FR_OK)
	{
		res = f_unlink(path);
		if(res != FR_OK)
			return res;
	}

	preset_makeFileNameSave(fileName, kitNr, FEXT_SOUND, name);
	snprintf(path, sizeof(path), "%s/%s", folder, fileName);
	res = preset_fopenWrite(path);
	if(res != FR_OK)
		return res;
	res = preset_fwrite((FIL*)&SDFile, name, 8, &bytesWritten);
	if(res == FR_OK)
		res = preset_fwrite((FIL*)&SDFile, params, END_OF_SOUND_PARAMETERS, &bytesWritten);
	if(res == FR_OK)
		res = preset_writeEngines(engines);
	return preset_fcloseWrite();
}
//----------------------------------------------------
void preset_loadKitData(const uint8_t* params, const uint8_t* engines, const char* name)
{
	memcpy(activeParameters, params, END_OF_SOUND_PARAMETERS);
	memcpy(kitEngines, engines, NUM_VOICE_SLOTS);
	memcpy(preset_otherKitName, name, 8);
	preset_otherKitLoaded = 1;
	preset_sendDrumsetParameters();
}
//----------------------------------------------------
const char* preset_activeKitName(void)
{
	return preset_otherKitLoaded ? preset_otherKitName : savefileNames[menu_loadedPresetNr[CURRENT_KIT]][KIT_NAMES];
}
//----------------------------------------------------
void preset_loadProjectNames(uint8_t projectNr)
{
	FRESULT res;
	FILINFO fno;

	//change project folder
	preset_projectFolderName(currentFolder, projectNr);

	res = f_stat(currentFolder, &fno);
	if(res == FR_NO_FILE)
	{
		sprintf((void*)savefileNames[projectNr][PROJECT_NAMES], "Empty");
		return;
	}
	else
	{
		f_chdir(currentFolder);
	}

	//Load Project name

		UINT bytesRead;

		//open the file
		res = f_open((FIL*)&SDFile,"prj.nfo",FA_OPEN_EXISTING | FA_READ);

		//read preset name
		//first the preset name
		res=f_read((FIL*)&SDFile,(void*)savefileNames[projectNr][PROJECT_NAMES],8,&bytesRead);
		if(bytesRead != 8) {
			res=FR_DISK_ERR;
		}
		if(preset_isNameEmpty(savefileNames[projectNr][PROJECT_NAMES]))
		{
			sprintf((void*)savefileNames[projectNr][PROJECT_NAMES], "Empty");
		}

		f_close((FIL*)&SDFile);
		preset_initTmpFolder();

}
//----------------------------------------------------
FRESULT preset_loadProject(uint8_t presetNr, uint8_t clearTmp)
{
	FRESULT res;
	FILINFO fno;
	char text[17];

	lcd_process_display_interrupt = 0;
	while(sendDisplayBuffer() != 0);

#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
	HAL_TIM_Base_Start_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif

	if(clearTmp)
	{
		preset_clearTmpFolder();
	}
	/*
	MX_FATFS_ReInit();
	f_mount(0, (TCHAR const*)SDPath, 0); //un-mount
	memset((FATFS*)&SDFatFS,0,sizeof(FATFS)); //clear FS object to force reinit

	res = f_mount((FATFS*)&SDFatFS, (TCHAR const*)SDPath,1);
	if( res != FR_OK)
	{
		return res;
	}
*/

	lcd_clear();
	lcd_home();
	lcd_string(PSTR("Loading Project"));
	lcd_setcursor(0,2);
	lcd_string(PSTR("0%"));

	//change project folder
	preset_projectFolderName(currentFolder, presetNr);

	res = f_stat(currentFolder, &fno);
	if(res == FR_NO_FILE)
	{
		res = f_mkdir(currentFolder);
		if(res == FR_OK)
		{
			f_chdir(currentFolder);
		}
		else
		{
			lcd_process_display_interrupt = 1;
			preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
			return res;
		}
	}
	else
	{
		res = f_chdir(currentFolder);
		if (res != FR_OK)
		{
			lcd_process_display_interrupt = 1;
			preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
			return res;
		}
	}
	//load data

#if 1

	//Load Project name

	UINT bytesRead;

	//open the file
	res = f_open((FIL*)&SDFile,"prj.nfo",FA_OPEN_EXISTING | FA_READ);
	if (res != FR_OK)
	{
		lcd_process_display_interrupt = 1;
		preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
		return res;
	}
	//read preset name
	//first the preset name
	res = f_read((FIL*)&SDFile,(void*)savefileNames[presetNr][PROJECT_NAMES],8,&bytesRead);
	if (res != FR_OK)
	{
		lcd_process_display_interrupt = 1;
		preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
		f_close((FIL*)&SDFile);
		return res;
	}

	if(bytesRead != 8) {
		sprintf((void*)savefileNames[presetNr][PROJECT_NAMES], "Empty");
		/*
		//res=FR_DISK_ERR;
		lcd_process_display_interrupt = 1;
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
		f_close((FIL*)&SDFile);
		return res;
		*/
	}

	else if(preset_isNameEmpty(savefileNames[presetNr][PROJECT_NAMES]))
	{
		sprintf((void*)savefileNames[presetNr][PROJECT_NAMES], "Empty");
	}
	//close the file
	f_close((FIL*)&SDFile);

	//load globals
	preset_loadGlobals();
	//load all drumkits, patterns and songs
	for(uint8_t i=0;i<NUM_KIT_SLOTS;i++)
	{

		sprintf(text, "%d%%", (uint8_t)(i/(float)NUM_KIT_SLOTS * 100.f));
		lcd_setcursor(0,2);
		lcd_string(text);

		//FR_NO_FILE just means an incomplete project and is no reason to abort!
		res = preset_loadDrumsetSD(i, 0);
		if ((res != FR_OK) && (res != FR_NO_FILE))
		{
			lcd_process_display_interrupt = 1;
			preset_initTmpFolder();
			f_close((FIL*)&SDFile);
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
			return res;
		}
		res = preset_loadPatternSD(i);
		if ((res != FR_OK) && (res != FR_NO_FILE))
		{
			f_close((FIL*)&SDFile);
			lcd_process_display_interrupt = 1;
			preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
			return res;
		}
		res = preset_loadSongSD(i);
		if ((res != FR_OK) && (res != FR_NO_FILE))
		{
			f_close((FIL*)&SDFile);
			lcd_process_display_interrupt = 1;
			preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif
			return res;
		}
	}

	res = FR_OK;

	//init morph target to peset 0
	morphParameters = mainParameters[0];


	//if pattern change mode is 'auto' load corresponding kit
	if(activeParameters[PAR_PATTERN_MODE] >= 1)
	{
		activeParameters[PAR_PATTERN_MODE] = 1; //prevent a value of 2 (skip next autoload) being saved with the project
		preset_loadDrumsetRam(seq_patternSet.kitNr[seq_activePattern], 0, 1);
		menu_selectedPresetNr[SAVE_TYPE_KIT] = seq_patternSet.kitNr[seq_activePattern];
		menu_loadedPresetNr[CURRENT_KIT] = seq_patternSet.kitNr[seq_activePattern];
		//refresh backup buffer with new kit, to prevent corruption when changin patterns with PCM=auto
		seq_copyPatternToBackupBuffer(seq_activePattern);
	}
	else
	{
		//init to preset 1
		//TODO maybe init to current active preset nr???
		preset_loadDrumsetRam(0,0, 1);
		menu_selectedPresetNr[SAVE_TYPE_KIT] = 0;
		menu_loadedPresetNr[CURRENT_KIT] = 0;
		seq_copyPatternToBackupBuffer(seq_activePattern);

	}

	preset_saveLastUsedProjectNrToSD(presetNr);

	//change pattern to pat 1
	seq_setNextPattern(0);

	lcd_process_display_interrupt = 1;
	preset_initTmpFolder();
#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
			HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
#else

#endif

	return res;
#else
#if USE_SD_CARD
	//filename in 8.3  format
	char filename[GEN_BUF_LEN];
	UINT bytesRead;
	uint16_t remain;
	uint8_t siz;
	uint8_t version=0;
	//sprintf(filename,"p%03d.snd",presetNr);
	if(isAll)
		preset_makeFileName(filename,presetNr,FEXT_ALL);
	else
		preset_makeFileName(filename,presetNr,FEXT_PROJ);

	//open the file
	FRESULT res = f_open((FIL*)&SDFile,filename,FA_OPEN_EXISTING | FA_READ);

	if(res!=FR_OK)
		return; //file open error... maybe the file does not exist?

	//first the preset name
	f_read((FIL*)&SDFile,(void*)savefileNames[KIT_NAMES][presetNr],8,&bytesRead);
	if(!bytesRead)
		goto closeFile;

	// read version number and make sure its valid
	f_read((FIL*)&SDFile,&version,1,&bytesRead);
	if(!bytesRead || version > FILE_VERSION)
		goto closeFile;

	if(isAll){
		// read global data
		if(!preset_readGlobalData())
			goto closeFile;
		// size of padding after globals
		remain=	64- (NUM_PARAMS - PAR_BEGINNING_OF_GLOBALS);

	} else {
		// read bpm
		f_read((FIL*)&SDFile,&activeParameters[PAR_BPM],1,&bytesRead);
		if(!bytesRead)
			goto closeFile;

		// size of padding after these
		if(version>1) {
			// read pat reset bit
			f_read((FIL*)&SDFile,&activeParameters[PAR_BAR_RESET_MODE],1,&bytesRead);
			if(!bytesRead)
				goto closeFile;
			remain=	64- (1+1);
		} else
			remain=0; // version 1 didn't have padding for performances
	}

	while(remain){
		if(remain < GEN_BUF_LEN)
			siz=(uint8_t)remain;
		else
			siz=GEN_BUF_LEN;
		f_read((FIL*)&SDFile, filename, siz, &bytesRead);
		if(bytesRead !=siz)
			goto closeFile;
		remain -=siz;
	}



	// read kit data
	res=preset_readDrumsetData(presetNr, 0);
	if(res!=FR_OK)
		goto closeFile;

	// read padding
	remain = 512 - (END_OF_SOUND_PARAMETERS);
	while(remain){
		if(remain < GEN_BUF_LEN)
			siz=(uint8_t)remain;
		else
			siz=GEN_BUF_LEN;
		f_read((FIL*)&SDFile, filename, siz, &bytesRead);
		if(bytesRead !=siz)
			goto closeFile;
		remain -=siz;
	}


	// read pattern data
	/*//temporary removed
	if(!preset_readPatternData())
		goto closeFile;
		*/



closeFile:
	//close the file handle
	f_close((FIL*)&SDFile);

	// send drumset parameters to back
	preset_sendDrumsetParameters();

	// send global params
	menu_sendAllGlobals();

	//force complete repaint
	menu_repaintAll();

#else
	frontPanel_sendData(PRESET,PRESET_LOAD,presetNr);
#endif
#endif
}
//----------------------------------------------------

static void preset_makeFileNameOld(char *buf, uint8_t num, uint8_t type)
{

	const char *ext;
	buf[0]='p';
	numtostrpu(&buf[1],num,'0');
	switch(type) {
	case FEXT_SOUND:
		ext=PSTR(".snd");
		break;
	case FEXT_PAT:
		ext=PSTR(".pat");
		break;
	case FEXT_ALL:
		ext=PSTR(".all");
		break;
	default:
	case FEXT_PROJ:
		ext=PSTR(".prj");
		break;

	case FEXT_SONG:
		ext=PSTR(".sng");
		break;
	}
	strcpy(&buf[4],ext);

};


//wildcard for the files of a slot: 00-*.ext
static void preset_makeFileNamePattern(char *buf, uint8_t num, uint8_t type)
{
	const char *ext;
	numtostrpu_2Bytes(&buf[0],num,'0');
	buf[2] = '-';
	buf[3] = '*';

	switch(type) {
	case FEXT_SOUND:
		ext=PSTR(".snd");
		break;
	case FEXT_PAT:
		ext=PSTR(".pat");
		break;
	case FEXT_ALL:
		ext=PSTR(".all");
		break;
	default:
	case FEXT_PROJ:
		ext=PSTR(".prj");
		break;

	case FEXT_SONG:
		ext=PSTR(".sng");
		break;
	}
	strcpy(&buf[4],ext);
	buf[8] = 0;
}

//load uses wildcard to find a name of the pattern 00-*.ext
static FRESULT preset_makeFileNameLoad(char *buf, uint8_t num, uint8_t type)
{

	FRESULT fr;     /* Return value */
    DIR dj;         /* Directory object */
    FILINFO fno;    /* File information */

	preset_makeFileNamePattern(buf, num, type);

    fr = f_findfirst(&dj, &fno, "", buf);  /* Start to search for preset files */
    f_closedir(&dj);
    if(fr == FR_OK && fno.fname[0])
    {
    	fr = f_open((FIL*)&SDFile,fno.fname,FA_OPEN_EXISTING | FA_READ);
    	return fr;
    }

    return FR_NO_FILE;
}

//the file name contains the slot's name: after saving, remove the slot's files with other names
//(otherwise loading may find the old file first)
static FRESULT preset_removeOtherFiles(uint8_t num, uint8_t type, const char *keep)
{
	char pattern[9];
	char name[13];
	DIR dj;
	FILINFO fno;
	FRESULT fr;

	if(preset_writingNewFolder)
		return FR_OK;
	preset_makeFileNamePattern(pattern, num, type);
	while(1)
	{
		uint8_t found = 0;
		fr = f_findfirst(&dj, &fno, "", pattern);
		while(fr == FR_OK && fno.fname[0])
		{
			if(strcasecmp(fno.fname, keep) != 0)
			{
				strcpy(name, fno.fname);
				found = 1;
				break;
			}
			fr = f_findnext(&dj, &fno);
		}
		f_closedir(&dj);
		if(!found)
			return fr;
		//only one open object at a time (_FS_LOCK): unlink after closing the directory
		fr = f_unlink(name);
		if(fr != FR_OK)
			return fr;
	}
}

void clearIllegalCharacters(char *input, char* output, uint8_t sz)
{
	for(uint8_t i=0; i<sz; i++)
	{
		if((input[i] >= 'a') && (input[i] <= 'z')) output[i] = input[i];
		else if((input[i] >= 'A') && (input[i] <= 'Z')) output[i] = input[i];
		else if((input[i] >= '0') && (input[i] <= '9')) output[i] = input[i];
		//! # $ % & ' ( ) - @ ^ _ ` { } ~
		else if(input[i] == ' ')  output[i] = '_';
		else if(input[i] == '!')  output[i] = input[i];
		else if(input[i] == '#')  output[i] = input[i];
		else if(input[i] == '$')  output[i] = input[i];
		else if(input[i] == '%')  output[i] = input[i];
		else if(input[i] == '&')  output[i] = input[i];
		else if(input[i] == '(')  output[i] = input[i];
		else if(input[i] == ')')  output[i] = input[i];
		else if(input[i] == '-')  output[i] = input[i];
		else if(input[i] == '@')  output[i] = input[i];
		else if(input[i] == '^')  output[i] = input[i];
		else if(input[i] == '_')  output[i] = input[i];
		else if(input[i] == '`')  output[i] = input[i];
		else if(input[i] == '{')  output[i] = input[i];
		else if(input[i] == '}')  output[i] = input[i];
		else if(input[i] == '~')  output[i] = input[i];
		else if(input[i] == 0)  output[i] = input[i];
		else output[i] = '_';
	}

}
//save uses the kit name to create a filename eg 00-NAME.ext
static void preset_makeFileNameSave(char *buf, uint8_t num, uint8_t type, char *name)
{

	char clearedName[9];
	clearedName[8] = 0;
	clearIllegalCharacters(name , clearedName, 8);
	//name scheme: 01-NAME.ext
	const char *ext;
	numtostrpu_2Bytes(&buf[0],num,'0');
	buf[2] = '-';

	//copy the first 5 characters of the name to the filename if not empty
	uint8_t i=0;
	for( ; i<5; i++)
	{
		if(clearedName[i] != 0)
		{
			if(clearedName[i] == ' ')
				buf[3+i] = '_';
			else
				buf[3+i] = clearedName[i];
		}
		else
		{
			break;
		}
	}

	switch(type) {
	case FEXT_SOUND:
		ext=PSTR(".snd");
		break;
	case FEXT_PAT:
		ext=PSTR(".pat");
		break;
	case FEXT_ALL:
		ext=PSTR(".all");
		break;
	default:
	case FEXT_PROJ:
		ext=PSTR(".prj");
		break;

	case FEXT_SONG:
		ext=PSTR(".sng");
		break;
	}
	strcpy(&buf[3+i],ext);
}

//----------------------------------------------------
/*
static void preset_makeFileNameTemp(char *buf, uint8_t num, uint8_t type)
{
	const char *ext;
	buf[0]='t';
	numtostrpu(&buf[1],num,'0');
	switch(type) {
	case 0:
		ext=PSTR(".snd");
		break;
	case 1:
		ext=PSTR(".pat");
		break;
	case 2:
		ext=PSTR(".all");
		break;
	default:
	case 3:
		ext=PSTR(".prf");
		break;
	}
	strcpy(&buf[4],ext);
}
*/
