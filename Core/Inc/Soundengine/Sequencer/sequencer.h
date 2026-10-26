/*
 * sequencer.h
 *
 *  Created on: 11.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#ifndef SEQUENCER_H_
#define SEQUENCER_H_



#include "stm32f7xx_hal.h"
#include "Soundengine/globals.h"
#include "Preset/SeqStep.h"
#include "Parameters.h"

//Step automation stores its target like a received MIDI CC: parameters 1-127 as
//parameter+1 (the CC number), parameters >= 128 as the parameter number; 0 and
//0xff (NO_AUTOMATION) mean no target. Returns the target parameter, PAR_NONE if none.
static inline uint16_t seq_automationTarget(const uint8_t stored)
{
	if(stored == 0 || stored == 0xff)
		return PAR_NONE;
	return stored < 128 ? (uint16_t)(stored - 1) : stored;
}

 /**<
  * we have 6 voices
  * 3 drums
  * 1 snare/claps
  * 1 cymbal/snare
  * 1 hiHat
  * track 7 is the open hh... it triggers the hihat voice but with longer decay. it chokes the closed hihat*/
#define NUM_TRACKS 7
#define NUM_PATTERN 16*4 //8
#define NUM_STEPS 64
#define NUM_BARS 4	//since we access main steps as 64 bit union, other values than 4 will break the union approach
#define MAX_PATTERN 64

#define MAX_SONG_LENGTH 64
#define SONG_END MAX_PATTERN
#define SONG_LOOP SONG_END+1

#define SEQ_DEFAULT_NOTE 60

#define STEP_ACTIVE_MASK 0x80
#define STEP_VOLUME_MASK 0x7f

// **PATROT these are not used anymore
//#define PATTERN_END_MASK 0x7f
//#define PATTERN_END 0x80

//#define SEQ_NEXT_RANDOM 		64
//#define SEQ_NEXT_RANDOM_PREV 	65

#define ROLL_VOLUME 100

enum Seq_QuantisationEnum
{
	NO_QUANTISATION,
	QUANT_8,
	QUANT_16,
	QUANT_32,
	QUANT_64,
};
/*
typedef struct StepStruct
{
	uint8_t 	volume;		// 0-127 volume -> 0x7f => lower 7 bit, upper bit => active
	uint8_t  	prob;		//step probability (todo we have one free bit here)
	uint8_t		note;		//midi note value 0-127 -> 0x7f, todo upper bit is now free for other usages

	//parameter automation
	uint8_t 	param1Nr;
	uint8_t 	param1Val;

	uint8_t 	param2Nr;
	uint8_t 	param2Val;

}Step;
*/

typedef union
{
	uint16_t word[NUM_BARS];
	uint64_t quadword;
}ActiveSteps;

/*
typedef struct PatternSettingsStruct
{
	uint8_t 	changeBar;		// change on every Nth bar to the next pattern
	uint8_t  	nextPattern;	// [0:65] (0-63) are the 64 patterns, (64) is random previous, (65) is random all
}PatternSetting;
*/



typedef struct PatternSetStruct
{
	Step seq_subStepPattern[NUM_PATTERN][NUM_TRACKS][NUM_STEPS];	//contains step data
	StepTiming seq_stepTiming[NUM_PATTERN][NUM_TRACKS][NUM_STEPS];
	uint16_t seq_mainSteps[NUM_BARS][NUM_PATTERN][NUM_TRACKS];		//bits represent if step is active
	//PatternSetting seq_patternSettings[NUM_PATTERN];				//pattern chaining info
	uint8_t length[NUM_PATTERN][NUM_TRACKS];	//only used in euklid generator?
	uint8_t shuffle[NUM_PATTERN];
	uint8_t kitNr[NUM_PATTERN];
}PatternSet;

typedef struct PatternBackupBufferStruct
{
	Step seq_subStepPattern[NUM_TRACKS][NUM_STEPS];	//contains step data
	uint8_t seq_stepTiming[NUM_TRACKS][NUM_STEPS];
	uint16_t seq_mainSteps[NUM_BARS][NUM_TRACKS];		//bits represent if step is active
	//PatternSetting seq_patternSettings[NUM_PATTERN];				//pattern chaining info
	uint8_t length[NUM_TRACKS];	//only used in euklid generator?
	uint8_t shuffle;
	uint8_t kitNr;
}PatternBackupBuffer;

/*
typedef struct TempPatternStruct
{
	Step seq_subStepPattern[NUM_TRACKS][NUM_STEPS];
	uint16_t seq_mainSteps[NUM_BARS][NUM_TRACKS];
	//PatternSetting seq_patternSettings;
	uint8_t length[NUM_TRACKS]; // only used for length
}TempPattern;
*/
extern uint8_t seq_activePattern;
extern uint8_t seq_pendingPattern;
extern uint8_t seq_newPatternAvailable;
extern uint8_t seq_prescaleCounter;

//extern PatternSet* seq_activePatternSetPtr;

//I think this is a doomed approach and not fully implemented
//leave it to 0 and remove me in the future!!!
#define USE_PATTERN_EDIT_BUFFER 0


#if USE_PATTERN_EDIT_BUFFER
	extern PatternSet seq_patterns;
#else
	extern PatternSet seq_patternSet;
#endif

//used as a Pattern Backup Buffer
//keeps a copy of the pattern state when it was loaded
//when current pattern is saved to a new slot or changed without being saved, buffer is used to restore original state
extern PatternBackupBuffer patternBackupBuffer;
//extern TempPattern seq_tmpPattern;



/* array holding the song data
 * just a list of pattern numbers that are played one after another
 */
//#define NO_PRELOAD_PENDING 0xFF
extern uint8_t seq_songData[NUM_KIT_SLOTS][MAX_SONG_LENGTH];
extern uint8_t *seq_activeSong;
extern uint8_t seq_songPositionPointer;
//extern uint8_t seq_bankPreloadPendingFlag;			//pattern bank can't be switched in Audio/Seq IRQ
												//so the IRQ sets this flag to let the mainloop do the pre loading

extern uint8_t seq_selectedStep;

extern uint8_t seq_resetBarOnPatternChange;
//------------------------------------------------------------------------------
void seq_MidiSyncQueue(uint8_t cnt);
//------------------------------------------------------------------------------
uint8_t seq_isAutomationStepArmed();
//------------------------------------------------------------------------------
void seq_nextStep();
//------------------------------------------------------------------------------
void seq_processFlam(uint8_t idx);
//------------------------------------------------------------------------------
void seq_triggerVoice(uint8_t voiceNr, uint8_t vol, uint8_t note);
//------------------------------------------------------------------------------
void seq_setShuffle(float shuffle);
//------------------------------------------------------------------------------
void seq_setTrackLength(uint8_t trackNr, uint8_t length);
//------------------------------------------------------------------------------
uint8_t seq_getTrackLength(uint8_t trackNr);
//------------------------------------------------------------------------------
float seq_getDeltaTShuffleDelay(uint16_t bpm);
//------------------------------------------------------------------------------
//void seq_setTrackRotation(uint8_t trackNr, const uint8_t rot);
//------------------------------------------------------------------------------
//uint8_t seq_getTrackRotation(uint8_t trackNr);
//------------------------------------------------------------------------------
//void seq_activateTmpPattern();
//------------------------------------------------------------------------------
void seq_init();
//------------------------------------------------------------------------------
float seq_getElapsedStepTimePercent();
//------------------------------------------------------------------------------
/** call periodically to check if the next step has to be processed */
void seq_tick();
//------------------------------------------------------------------------------
void seq_armAutomationStep(uint8_t stepNr, uint8_t track,uint8_t isArmed);
//------------------------------------------------------------------------------
//void seq_resetDeltaAndTick();
//------------------------------------------------------------------------------
void seq_setDeltaT(float delta);
//------------------------------------------------------------------------------
void seq_triggerNextMasterStep(uint8_t stepSize);
//------------------------------------------------------------------------------
void seq_setBpm(uint16_t bpm);
//------------------------------------------------------------------------------
uint16_t seq_getBpm();
//------------------------------------------------------------------------------
void seq_sync();
//------------------------------------------------------------------------------
uint8_t seq_getMasterStepCnt();
//------------------------------------------------------------------------------
//void seq_nextStep();
//------------------------------------------------------------------------------
uint8_t seq_getExtSync();
//------------------------------------------------------------------------------
void seq_setQuantisation(uint8_t value);
//------------------------------------------------------------------------------
void seq_setExtSync(uint8_t isExt);
//------------------------------------------------------------------------------
/** switch to pattern patNr after the current pattern has finished*/
void seq_setNextPattern(const uint8_t patNr);
//------------------------------------------------------------------------------
void seq_toggleStep(uint8_t voice, uint8_t stepNr, uint8_t patternNr);
//------------------------------------------------------------------------------
void seq_toggleMainStep(uint8_t voice, uint8_t stepNr, uint8_t patternNr);
//------------------------------------------------------------------------------
void seq_setMainStep(uint8_t patternNr, uint8_t voice, uint8_t stepNr, uint8_t onOff);
//------------------------------------------------------------------------------
//void seq_setStep(uint8_t voice, uint8_t stepNr, uint8_t onOff);
//------------------------------------------------------------------------------
void seq_resetPosition();
//------------------------------------------------------------------------------
void seq_setRunning(uint8_t isRunning);
//------------------------------------------------------------------------------
void seq_setSongModeEnabled(uint8_t onOff);
//------------------------------------------------------------------------------
void seq_addPatternToSong(uint8_t patternNr);
//------------------------------------------------------------------------------
void seq_deleteRow();
//------------------------------------------------------------------------------
void seq_insertRow();
//------------------------------------------------------------------------------
uint8_t seq_isRunning();
//------------------------------------------------------------------------------
uint8_t seq_isStepActive(uint8_t voice, uint8_t stepNr, uint8_t patternNr);
//------------------------------------------------------------------------------
uint8_t seq_isMainStepActive(uint8_t voice, uint8_t mainStepNr, uint8_t pattern);
//------------------------------------------------------------------------------
/** mutes and unmutes a track [0..maxTrack]*/
void seq_setMute(uint8_t trackNr, uint8_t isMuted);
//------------------------------------------------------------------------------
uint8_t seq_isTrackMuted(uint8_t trackNr);
//------------------------------------------------------------------------------
/** send step data to front panel. the whole step struct for one step is transmitted*/
void seq_sendStepInfoToFront(uint16_t stepNr);
//------------------------------------------------------------------------------
//void seq_sendMainStepInfoToFront(uint16_t stepNr);
//------------------------------------------------------------------------------
void seq_setRoll(uint8_t voice, uint8_t onOff);
//------------------------------------------------------------------------------
void seq_setRollRate(uint8_t rate);
//------------------------------------------------------------------------------
/** add a note to the current pattern position*/
void seq_addNote(uint8_t trackNr,uint8_t vel, uint8_t note);
//------------------------------------------------------------------------------
void seq_setRecordingMode(uint8_t active);
//------------------------------------------------------------------------------
void seq_setErasingMode(uint8_t active);
//------------------------------------------------------------------------------
void seq_clearTrack(uint8_t trackNr, uint8_t pattern);
//------------------------------------------------------------------------------
void seq_clearAutomation(uint8_t trackNr, uint8_t pattern, uint8_t automTrack);
//------------------------------------------------------------------------------
void seq_clearPattern(uint8_t pattern);
//------------------------------------------------------------------------------
void seq_copyTrack(uint8_t srcNr, uint8_t dstNr, uint8_t pattern);
//------------------------------------------------------------------------------
void seq_copyPattern(uint8_t src, uint8_t dst);
//------------------------------------------------------------------------------
//selects the automation track (0:1) that is recorded to
void seq_setActiveAutomationTrack(uint8_t trackNr);
//------------------------------------------------------------------------------
void seq_recordAutomation(uint8_t voice, uint8_t dest, uint8_t value);
//------------------------------------------------------------------------------
void seq_copyPatternToBackupBuffer(uint8_t patternNr);
//------------------------------------------------------------------------------
void seq_copyBackupBufferToPattern(uint8_t patternNr);
//------------------------------------------------------------------------------
//void seq_copyEditBufferToRam(uint8_t patternNr);
//------------------------------------------------------------------------------
//uint8_t seq_isNextStepSyncStep();
//------------------------------------------------------------------------------
// send a note off for a channel if there is a note playing on that channel
// if 0xff is specified, send a note off on all channels that have a note playing
void seq_midiNoteOff(uint8_t chan);
//------------------------------------------------------------------------------
void seq_sendMidiNoteOn(const uint8_t channel, const uint8_t note, const uint8_t veloc);
//------------------------------------------------------------------------------
uint8_t seq_getStepIndex(uint8_t trackNr);
//------------------------------------------------------------------------------
PatternSet* get_seq_patternSet_p(void);
uint8_t seq_getActiveAutomationTrack(void);

#endif /* SEQUENCER_H_ */
