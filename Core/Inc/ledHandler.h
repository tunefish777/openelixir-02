/*
 * ledHandler.h
 *
 * Created: 25.04.2012 16:01:19
 *  Author: Julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */ 


#ifndef LEDHANDLER_H_
#define LEDHANDLER_H_


#include "IO/DIO.h"


void led_init();

//turn on 1 of the 8 selection LEDS corresponding to the active page number
// input = [0:7]
void led_setActivePage(uint8_t pageNr);

void led_setActiveVoiceLeds(uint8_t pattern);
void led_setActiveVoice(uint8_t voiceNr);

void led_setActiveSelectButton(uint8_t butNr);


/** set the state of all select mode 2 leds*/
void led_setMode2Leds(uint8_t value);

void led_setValue(uint8_t val, uint8_t ledNr);
void led_setValueTemp(uint8_t val, uint8_t ledNr);

//void led_getValue(uint8_t ledNr);

void led_clearAll();

/** toggle a led*/
void led_toggle(uint8_t ledNr);

/** toggle a led temporary (can be reset to original state via led_reset() function)*/
void led_toggleTemp(uint8_t ledNr);
/** reset the led state to its original value (undo for temp operations)*/
void led_reset(uint8_t ledNr);

//update BarButtons in pattern Mode
void led_initPatternSelectLeds();
//pattern load/save page: repaint if slots of the shown bank got steps or were cleared
void led_refreshPatternSelectLeds();

//update the sequencer leds with active steps
void led_updateSeqLeds();

void led_updateBarLeds();
/** clear all 16 seq leds*/
void led_clearSequencerLeds();

/** clear seq leds 9-16*/
void led_clearSequencerLeds9_16();
/** clear seq leds 1-8*/
void led_clearSequencerLeds1_8();

void led_clearSelectLeds();
void led_clearVoiceLeds();

void led_setMode2(uint8_t status);

uint8_t led_idx2SelectLed(uint8_t idx);

/**toggles the currently active step from the chase light indicator*/
void led_setActive_step(uint8_t stepNr);

void led_clearActive_step();

/**pulse a led.
this function flashes the LED once.
only NUM_OF_PULSABLE_LEDS (default 8) LEDs can be pulsed simultanously!*/
void led_pulseLed(uint8_t ledNr);

/** activate and deactivate blinking mode for LEDs*/
void led_setBlinkLed(const uint8_t ledNr, const uint8_t onOff);
/** blink 4 times as fast as led_setBlinkLed, turn it off with led_setBlinkLed(ledNr, 0)*/
void led_setFastBlinkLed(const uint8_t ledNr);
void led_clearAllBlinkLeds();

/**this function needs to be called periodically to check if it is time to turn of a pulsed led*/
void led_tickHandler();


void led_blinkHandler();
void led_resetBlinkTimer();

//some LED helper functions for non clustered groups in LXR V2
uint8_t led_idx2StepLed(uint8_t idx);
uint8_t led_idx2SelectLed(uint8_t idx);

enum LedNumbers
{
	LED_PAGE_UP,
	LED_VOICE1,
	LED_VOICE2,
	LED_VOICE3,
	LED_VOICE4,
	LED_VOICE5,
	LED_VOICE6,
	LED_VOICE7,


	LED_PART_SELECT5,
	LED_PART_SELECT6,
	LED_PART_SELECT7,
	LED_PART_SELECT8,
	LED_STEP16,
	LED_STEP15,
	LED_STEP14,
	LED_STEP13,

	LED_PART_SELECT1 ,
	LED_PART_SELECT2,
	LED_PART_SELECT3,
	LED_PART_SELECT4,
	LED_STEP12,
	LED_STEP11,
	LED_STEP10,
	LED_STEP9,

	LED_MODE1,
	LED_MODE2,
	LED_MODE3,
	LED_MODE4,
	LED_STEP8,
	LED_STEP7,
	LED_STEP6,
	LED_STEP5,
	
	LED_SHIFT,
	LED_START_STOP,
	LED_REC,
	LED_COPY,
	LED_STEP4,
	LED_STEP3,
	LED_STEP2,
	LED_STEP1,

	LED_PAGE_DOWN

};

#endif /* LEDHANDLER_H_ */
