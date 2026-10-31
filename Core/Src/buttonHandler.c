/*
 * buttonHandler.c
 *
 * Created: 24.04.2012 16:04:29
 *  Author: Julian
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */
#include <frontPanelParser_ui.h>
#include "buttonHandler.h"
#include "menu/menu.h"
#include "ledHandler.h"
#include "lcd.h"
#include <stdio.h>
#include "IO/DIO.h"
#include "menu/screensaver.h"
#include "menu/copyClearTools.h"
#include "menu/engineMenu.h"
#include "menu/voiceLoad.h"
#include "config.h"
#include "front.h"
#include "Soundengine/globals.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "Preset/PresetManager.h"
#include "Soundengine/FxBus.h"
#include "Soundengine/CpuMeter.h"

//used to remember the last active page when entering the sequencer mode
//needed to return to the previously shown page when seq mode button is released
//static uint8_t lastActivePage = 0;
static uint8_t lastActiveSubPage = 0;
/** selected step for sequencer mode*/
uint8_t buttonHandler_selectedStep = 0; //TODO is this the same as parameter PAR_ACTIVE_STEP
static uint8_t selectedStepLed = LED_STEP1;

static uint32_t buttonHandler_buttonTimer = 0;

#define TIMER_ACTION_OCCURED -2
static int8_t buttonHandler_buttonTimerStepNr = NO_STEP_SELECTED;
volatile uint16_t buttonHandler_originalParameter[2];//saves parameter number for step automation reset (stgep assign)
uint8_t buttonHandler_originalValue[2];//saves the parameter value for reset
uint8_t buttonHandler_resetLock1 = 0;
uint8_t buttonHandler_resetLock2 = 0;
// uint8_t buttonHandler_resetLock[NUM_AUTOMATION_PAR];

volatile struct {
	unsigned selectButtonMode :4; // 0-3 for unshifted, 4-7 for shifted + extra modes (like pattern select)
	unsigned seqRunning :1;
	unsigned seqRecording :1;
	unsigned seqErasing :1; // **RECORD

} buttonHandler_stateMemory;

static uint8_t buttonHandler_mutedVoices = 0;
volatile static int8_t buttonHandler_armedAutomationStep = NO_STEP_SELECTED;

#define ARM_AUTOMATION		0x40
#define DISARM_AUTOMATION	0x00

//--------------------------------------------------------
static void buttonHandler_armTimerActionStep(int8_t stepNr) {
#if 0
	//check if sub or main step
	const uint8_t isMainStep = ((stepNr % 8) == 0);
	buttonHandler_armedAutomationStep = stepNr;

	if (isMainStep) {
		const uint8_t mainStepNr = (uint8_t) (stepNr / 8);
		led_setBlinkLed( led_idx2StepLed(mainStepNr), 1);
	} else {
		const uint8_t selectButtonNr = (uint8_t) (stepNr % 8);
		led_setBlinkLed(led_idx2SelectLed(selectButtonNr), 1);
	}

	frontPanel_sendData(ARM_AUTOMATION_STEP, (uint8_t) (stepNr),
			menu_getActiveVoice() | ARM_AUTOMATION);
#else
	buttonHandler_armedAutomationStep = stepNr;
	led_setBlinkLed( led_idx2StepLed(stepNr%16), 1);

	frontPanel_sendData(ARM_AUTOMATION_STEP, (uint8_t) (stepNr),
		menu_getActiveVoice() | ARM_AUTOMATION);

#endif

  //////////////////////
  // PatternSet* patSet = get_seq_patternSet_p();
  // uint8_t parNr;
  // for (int i = 0; i < NUM_AUTOMATION_PAR; i++)
  // {
  //   parNr = patSet->seq_subStepPattern[seq_activePattern][0][stepNr].paramNr[i] - 1;
  //   if (parNr != 254)
  //   {
  //     buttonHandler_originalValue[i] = activeParameters[parNr];
  //     activeParameters[parNr] = patSet->seq_subStepPattern[seq_activePattern][0][stepNr].paramVal[i];

  //     buttonHandler_originalParameter[i] = parNr;
  //     buttonHandler_resetLock[i] = 1;
  //   }
  // }

  /////////////////////

  //the held step of the active track (was track 0, voice 1, whatever voice was selected);
  //the target decoded like the sequencer does (was -1 for all, wrong for parameters >= 128)
  PatternSet* patSet = get_seq_patternSet_p();
  const Step* step = &patSet->seq_subStepPattern[seq_activePattern][menu_getActiveVoice()][stepNr];
  uint16_t parNr = seq_automationTarget(step->param1Nr);

  if (parNr != PAR_NONE)
  {
    buttonHandler_originalValue[0] = activeParameters[parNr];
    activeParameters[parNr] = step->param1Val;

    buttonHandler_originalParameter[0] = parNr;
    buttonHandler_resetLock1 = 1;
  }

  parNr = seq_automationTarget(step->param2Nr);

  if (parNr != PAR_NONE)
  {
    buttonHandler_originalValue[1] = activeParameters[parNr];
    activeParameters[parNr] = step->param2Val;
  
    buttonHandler_originalParameter[1] = parNr;
    buttonHandler_resetLock2 = 1;
  }
  ///////////////

  menu_repaintAll();

}
//--------------------------------------------------------
static void buttonHandler_disarmTimerActionStep() {
  ///////
  // buttonHandler_armedAutomationStep != NO_STEP_SELECTED;
  ///////
	if (buttonHandler_armedAutomationStep != NO_STEP_SELECTED)
  {

    led_setBlinkLed(led_idx2StepLed(buttonHandler_armedAutomationStep%16), 0);
    
    /////////
    // for (int i = 0; i < NUM_AUTOMATION_PAR; i++)
    // {
    //   if (buttonHandler_resetLock[i] == 1)
    //   {
    //     activeParameters[buttonHandler_originalParameter[i]] = buttonHandler_originalValue[i];
    //     break;
    //   }
    // }
    ///////
		//revert to original sound
		//make changes temporary while an automation step is armed - revert to original value
		if (buttonHandler_resetLock1 == 1) {
			activeParameters[buttonHandler_originalParameter[0]] =
					buttonHandler_originalValue[0];
      // memcpy(activeParameters, buttonHandler_originalValue, NUM_PARAMS);
		}
    if (buttonHandler_resetLock2 == 1) {
      activeParameters[buttonHandler_originalParameter[1]] =
          buttonHandler_originalValue[1];
      // memcpy(activeParameters, buttonHandler_originalValue, NUM_PARAMS);
		}
    /////////

		buttonHandler_armedAutomationStep = NO_STEP_SELECTED;
		frontPanel_sendData(ARM_AUTOMATION_STEP, 0, DISARM_AUTOMATION);

    /////////
    // for (int i = 0; i < NUM_AUTOMATION_PAR; i++)
    // {
    //   if (buttonHandler_resetLock[i] == 1)
    //   {
    //     buttonHandler_resetLock[i] = 0;

    //     if (buttonHandler_originalParameter[i] != 0)
    //     {
    //       //&revert to original value
    //       if (buttonHandler_originalParameter[i] < 128) // => Sound Parameter
    //       {
    //         frontPanel_sendData(MIDI_CC,
    //             (uint8_t) (buttonHandler_originalParameter[i]),
    //             buttonHandler_originalValue[i]);
    //       } 
    //       else if (buttonHandler_originalParameter[i] >= 128 && (buttonHandler_originalParameter[i] < END_OF_SOUND_PARAMETERS)) // => Sound Parameter above 127
    //       {
    //         frontPanel_sendData(CC_2,
    //         (uint8_t) (buttonHandler_originalParameter[i] - 128),
    //         buttonHandler_originalValue[i]);
    //       } 
    //       else
    //       {
    //         menu_parseGlobalParam(buttonHandler_originalParameter[i],
    //         activeParameters[buttonHandler_originalParameter[i]]);
    //       }
    //       menu_repaintAll();
    //     }
    //   }
    // }

    /////////
    if (buttonHandler_resetLock1 == 1)
    {
      buttonHandler_resetLock1 = 0;

      if (buttonHandler_originalParameter[0] != 0)
      {
        //&revert to original value
        if (buttonHandler_originalParameter[0] < 128) // => Sound Parameter
        {
          frontPanel_sendData(MIDI_CC,
              (uint8_t) (buttonHandler_originalParameter[0]),
              buttonHandler_originalValue[0]);
        } 
        else if (buttonHandler_originalParameter[0] >= 128 && (buttonHandler_originalParameter[0] < END_OF_SOUND_PARAMETERS)) // => Sound Parameter above 127
        {
          frontPanel_sendData(CC_2,
          (uint8_t) (buttonHandler_originalParameter[0] - 128),
          buttonHandler_originalValue[0]);
        } 
        else
        {
          menu_parseGlobalParam(buttonHandler_originalParameter[0],
          activeParameters[buttonHandler_originalParameter[0]]);
        }
        menu_repaintAll();
      }
    }

    if (buttonHandler_resetLock2 == 1)
    {
      buttonHandler_resetLock2 = 0;

      if (buttonHandler_originalParameter[1] != 0)
      {
        if (buttonHandler_originalParameter[1] < 128) // => Sound Parameter
        {
          frontPanel_sendData(MIDI_CC,
          (uint8_t) (buttonHandler_originalParameter[1]),
          buttonHandler_originalValue[1]);
        }
        else if (buttonHandler_originalParameter[1] >= 128 && (buttonHandler_originalParameter[1] < END_OF_SOUND_PARAMETERS)) // => Sound Parameter above 127
        {
          frontPanel_sendData(CC_2,
          (uint8_t) (buttonHandler_originalParameter[1] - 128),
          buttonHandler_originalValue[1]);
        }
        else
        {
          menu_parseGlobalParam(buttonHandler_originalParameter[1],
          activeParameters[buttonHandler_originalParameter[1]]);
        }
        menu_repaintAll();
      }
		}

		//free both lock slots for the next held step (they stayed taken, so the
		//next lock took the "both taken" path in menu.c and wiped automation)
		buttonHandler_originalParameter[0] = 0;
		buttonHandler_originalParameter[1] = 0;
		return;
	}

  /////////

	buttonHandler_armedAutomationStep = NO_STEP_SELECTED;
	frontPanel_sendData(ARM_AUTOMATION_STEP, 0, DISARM_AUTOMATION);

}

//--------------------------------------------------------
int8_t buttonHandler_getArmedAutomationStep() {
	return buttonHandler_armedAutomationStep;
}
;
//--------------------------------------------------------
static uint8_t buttonHandler_TimerActionOccured() {
	buttonHandler_disarmTimerActionStep();
	if (buttonHandler_buttonTimerStepNr == TIMER_ACTION_OCCURED) //NO_STEP_SELECTED)
	{
		//a timed action appeared -> do nothing
		return 1;
	}
	buttonHandler_buttonTimerStepNr = NO_STEP_SELECTED;
	return 0;
}
static void buttonHandler_setTimeraction(uint8_t buttonNr) {
	buttonHandler_buttonTimer = systick_ticks + BUTTON_TIMEOUT;
	buttonHandler_buttonTimerStepNr = (int8_t) buttonNr;

}
//--------------------------------------------------------
/**returns 1 is the mode 2 select button is pressed*/
/*
 uint8_t buttonHandler_isModeButtonPressed()
 {
 const uint8_t arrayPos	= BUT_MODE/8;
 const uint8_t bitPos	= BUT_MODE&7;

 if(din_inputData[arrayPos] & (1<<bitPos))
 {
 return 0;
 }
 return 1;
 }
 */


//--------------------------------------------------------
//periodically called handler for timed actions
//-> hold a step button for a long time to select/rec automation
void buttonHandler_tick() {
	if ((systick_ticks > buttonHandler_buttonTimer)) {
		if (buttonHandler_buttonTimerStepNr >= 0) //!=NO_STEP_SELECTED)
		{
			//select step
			buttonHandler_armTimerActionStep(buttonHandler_buttonTimerStepNr);
			//reset
			buttonHandler_buttonTimerStepNr = TIMER_ACTION_OCCURED; //NO_STEP_SELECTED;
		}
	}
}

//--------------------------------------------------------
static void buttonHandler_updateSubSteps() {
	led_clearSelectLeds();
#if 0
	//query current sequencer step states and light up the corresponding leds
	uint8_t trackNr = menu_getActiveVoice(); //max 6 => 0x6 = 0b110
	uint8_t patternNr = menu_getViewedPattern(); //max 7 => 0x07 = 0b111
	uint8_t value = (uint8_t) ((trackNr << 4) | (patternNr & 0x7));
	frontPanel_sendData(LED_CC, LED_QUERY_SEQ_TRACK, value);
#else
	led_updateSeqLeds();
#endif
}
//--------------------------------------------------------
/*
 -> change display to show selected step options (volume, probability, note etc...)
 -> seq buttons select step
 -> selected step is illuminated on the seq buttons
 -> select buttons show 8 sub steps for the selected step
 */
static void buttonHandler_enterSeqModeStepMode() {
//	lastActivePage = menu_activePage;
//	lastActiveSubPage = menu_getSubPage();
	menu_switchSubPage(0, 1);
	menu_switchPage(SEQ_PAGE);

	buttonHandler_updateSubSteps();

	led_setBlinkLed(selectedStepLed, 1);
}
//--------------------------------------------------------
/**returns 1 is the copy button is pressed*/
uint8_t buttonHandler_getCopy() {
	const uint8_t arrayPos = BUT_COPY / 8;
	const uint8_t bitPos = BUT_COPY & 7;

	if (din_inputData[arrayPos] & (1 << bitPos)) {
		return 1;
	}
	return 0;
}
//--------------------------------------------------------
/**returns 1 is the shift button is pressed*/
uint8_t buttonHandler_getShift() {
	const uint8_t arrayPos = BUT_SHIFT / 8;
	const uint8_t bitPos = BUT_SHIFT & 7;

	if (din_inputData[arrayPos] & (1 << bitPos)) {
		return 1;
	}
	return 0;
}
//--------------------------------------------------------
/**returns 1 is the Load button is pressed*/
uint8_t buttonHandler_getLoad() {
	const uint8_t arrayPos = BUT_MODE3 / 8;
	const uint8_t bitPos = BUT_MODE3 & 7;

	if (din_inputData[arrayPos] & (1 << bitPos)) {
		return 1;
	}
	return 0;
}
//--------------------------------------------------------
/**returns 1 is the Save button is pressed*/
uint8_t buttonHandler_getSave() {
	const uint8_t arrayPos = BUT_MODE4 / 8;
	const uint8_t bitPos = BUT_MODE4 & 7;

	if (din_inputData[arrayPos] & (1 << bitPos)) {
		return 1;
	}
	return 0;
}
//--------------------------------------------------------
static void buttonHandler_handleModeButtons(uint8_t mode) {

	if(mode != SELECT_MODE_SAVE && mode != SELECT_MODE_LOAD)
	{
		if (buttonHandler_getShift()) {
			//set the new mode
			buttonHandler_stateMemory.selectButtonMode = (uint8_t) ((mode + 4) & 0x07);
		} else {
			//set the new mode
			buttonHandler_stateMemory.selectButtonMode = (uint8_t) (mode & 0x07);
		}

		//kind of ugly...
		//load save should not alter selectButtonMode
		//but switching mode corrupts menu for pattern generator (neglects shift)
		mode = buttonHandler_stateMemory.selectButtonMode;

		led_clearAllBlinkLeds();
	}





	switch (mode) {
	case SELECT_MODE_PERF: //perf
	{


		//update the status LED
		led_setMode2(buttonHandler_stateMemory.selectButtonMode);

		led_clearSequencerLeds();
		led_clearSelectLeds();
//		led_initPatternSelectLeds();

		//set menu to perf page
		lastActiveSubPage = menu_getSubPage();
		menu_switchPage(PERF_PAGE);
		//menu_switchSubPage(0);
		menu_repaintAll();

	}
		break;

	case SELECT_MODE_STEP: //Step
		//update the status LED
		led_setMode2(buttonHandler_stateMemory.selectButtonMode);

		//menu_switchPage(menu_getActiveVoice());
		led_setActiveSelectButton(menu_getSubPage());
		buttonHandler_enterSeqModeStepMode();
		break;
	case SELECT_MODE_VOICE: //Edit/Voice

		if(menu_activePage < 7) //already in voice/edit mode
		{
			buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_STEP;
			//update the status LED
			led_setMode2(buttonHandler_stateMemory.selectButtonMode);

			//menu_switchPage(menu_getActiveVoice());
			led_setActiveSelectButton(menu_getSubPage());
			buttonHandler_enterSeqModeStepMode();

		}
		else
		{
			//update the status LED
			led_setMode2(buttonHandler_stateMemory.selectButtonMode);
			//set menu to voice page mode
			menu_switchPage(menu_getActiveVoice());
			led_setActiveSelectButton(menu_getSubPage());
			menu_resetActiveParameter();
		}
		break;

	case SELECT_MODE_LOAD: //Load
		led_setValue(1,LED_MODE3);
		break;

	case SELECT_MODE_SAVE: //Load
		led_setValue(1,LED_MODE4);
		break;

	case SELECT_MODE_MENU: //Config Menu
		//menu_switchPage(MENU_MIDI_PAGE);
		break;


	case SELECT_MODE_PAT_GEN: //pat gen
		//update the status LED
		led_setMode2(buttonHandler_stateMemory.selectButtonMode);
		frontPanel_sendData(SEQ_CC, SEQ_REQUEST_EUKLID_PARAMS,
				menu_getActiveVoice());
		menu_switchPage(EUKLID_PAGE);


		break;

	default:
		//update the status LED
		led_setMode2(buttonHandler_stateMemory.selectButtonMode);
		break;
	}
}
//--------------------------------------------------------
void buttonHandler_muteVoice(uint8_t voice, uint8_t isMuted) {
	DISABLE_CONV_WARNING
	if (isMuted) {
		buttonHandler_mutedVoices |= (1 << voice);

	} else {
		buttonHandler_mutedVoices &= ~(1 << voice);

	}
	END_DISABLE_CONV_WARNING

	//muted tracks are lit
	if (menu_muteModeActive) {
		led_setActiveVoiceLeds((uint8_t) (~buttonHandler_mutedVoices));
	}
}
;
//--------------------------------------------------------
void buttonHandler_showMuteLEDs() {
	led_setActiveVoiceLeds((uint8_t) (~buttonHandler_mutedVoices));
	menu_muteModeActive = 1;
}
//--------------------------------------------------------
static void buttonHandler_handleSelectButton(uint8_t buttonNr) {

	if (buttonHandler_getShift()) {

		switch (buttonHandler_stateMemory.selectButtonMode) {
		case SELECT_MODE_STEP:
		case SELECT_MODE_VOICE: {

			//nothing to do here... no more substeps

			/*
			//select buttons represent sub steps

			uint8_t stepNr = (uint8_t) (buttonHandler_selectedStep + buttonNr);
			uint8_t ledNr = led_idx2SelectLed(buttonNr);
			//toggle the led
			led_toggle(ledNr);
			//TODO lastActivePage zeigt nur auf den currentTrack wenn man im stepMode ist... tut nicht in den anderen modes :-/
			//set sequencer step on soundchip

			uint8_t trackNr = menu_getActiveVoice(); //max 6 => 0x6 = 0b110
			uint8_t patternNr = menu_getViewedPattern(); //max 7 => 0x07 = 0b111
			uint8_t value = (uint8_t) ((trackNr << 4) | (patternNr & 0x7));
			frontPanel_sendData(STEP_CC, value, stepNr);

			//request step parameters from sequencer
			frontPanel_sendData(SEQ_CC, SEQ_REQUEST_STEP_PARAMS, stepNr);

			parameter_values[PAR_ACTIVE_STEP] = stepNr;
			*/
		}
			break;
		case SELECT_MODE_PAT_GEN:
		case SELECT_MODE_PERF:
			/*
			//select shownPattern
			menu_setShownPattern(buttonNr);
			led_clearSelectLeds();
			led_clearAllBlinkLeds();
			led_setBlinkLed(led_idx2SelectLed(buttonNr), 1);
			//led_clearSequencerLeds();
			//query current sequencer step states and light up the corresponding leds
#if 0
			uint8_t trackNr = menu_getActiveVoice(); //max 6 => 0x6 = 0b110
			uint8_t patternNr = menu_getViewedPattern(); //max 7 => 0x07 = 0b111
			uint8_t value = (uint8_t) ((trackNr << 4) | (patternNr & 0x7));
			frontPanel_sendData(LED_CC, LED_QUERY_SEQ_TRACK, value);
#else
			led_updateSeqLeds();
#endif

			uint8_t patternNr = menu_getViewedPattern(); //max 7 => 0x07 = 0b111
			frontPanel_sendData(SEQ_CC, SEQ_REQUEST_PATTERN_PARAMS, patternNr);
			frontPanel_sendData(SEQ_CC, SEQ_REQUEST_EUKLID_PARAMS,
					menu_activePage);
					*/
			break;

		}
	} else {
		switch (buttonHandler_stateMemory.selectButtonMode) {

		case SELECT_MODE_STEP: {
			/*
			//select buttons represent sub steps

			uint8_t stepNr = (uint8_t) (buttonHandler_selectedStep + buttonNr);
			//uint8_t ledNr = LED_PART_SELECT1 + buttonNr;

			//request step parameters from sequencer
			frontPanel_sendData(SEQ_CC, SEQ_REQUEST_STEP_PARAMS, stepNr);

			parameter_values[PAR_ACTIVE_STEP] = stepNr;

			//buttonHandler_armTimerActionStep(stepNr);
			led_clearAllBlinkLeds();
			//re set the main step led
			led_setBlinkLed(led_idx2StepLed((stepNr / 8)), 1);

			const uint8_t selectButtonNr = stepNr % 8;
			led_setBlinkLed(led_idx2SelectLed(selectButtonNr), 1);
			*/

		}
			break;

		case SELECT_MODE_PERF:
			//switch menu back to voice editing
			menu_switchPage(menu_getActiveVoice());
		case SELECT_MODE_VOICE:

			//change sub page -> osc, filter, mod etc...
			menu_switchSubPage(buttonNr, 1);
			//go to 1st parameter on sub page
			menu_resetActiveParameter();
			led_setActiveSelectButton(buttonNr);
			menu_repaintAll();
			break;

#if 0
		case SELECT_MODE_PERF:

			//check if euklid or perf mode is active
			if (menu_getActivePage() == PERF_PAGE) {
				//todo
				//change pattern
				//if shift -> select pattern for edit

				if (buttonHandler_getShift()) {

				} else {
					/*
					//pattern

					//tell sequencer to change pattern
					frontPanel_sendData(SEQ_CC, SEQ_CHANGE_PAT, buttonNr);
					//flash corresponding LED until ACK (SEQ_CHANGE_PAT) received
					uint8_t ledNr = led_idx2SelectLed(buttonNr);
					led_clearAllBlinkLeds();

					led_setBlinkLed(ledNr, 1);

					//request the pattern info for the selected pattern (bar cnt, next...)
					//	frontPanel_sendData(SEQ_CC,SEQ_REQUEST_PATTERN_PARAMS,buttonNr);
*/
				}

			}
			//----- Euklid Mode -----
			else {
				/* //moved to voice button
				 //tell the sequencer the new active track
				 //TODO muss nicht f�r button 8 gesendet werden
				 frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,buttonNr);
				 //request the parameters for the new track
				 frontPanel_sendData(SEQ_CC,SEQ_REQUEST_EUKLID_PARAMS,buttonNr);
				 //the currently active track button is lit
				 led_setActivePage(buttonNr);
				 */

			}
			break;
#endif

		case SELECT_MODE_LOAD:
			//the currently active button is lit
			//led_setActivePage(buttonNr);

			//	menu_switchPage(LFO1_PAGE+buttonNr);
			break;

		case SELECT_MODE_FX:
		case SELECT_MODE_ENGINE:
			//return to voice mode
			buttonHandler_handleModeButtons(0);
			buttonHandler_handleSelectButton(buttonNr);
			break;

		case SELECT_MODE_SAVE:
		case SELECT_MODE_PAT_GEN:

		case SELECT_MODE_MENU:
		case SELECT_MODE_PATTERN:
		case SELECT_MODE_SONG:
			break;

		}
	}

}
//--------------------------------------------------------
/*
static void buttonHandler_leaveSeqModeStepMode() {
	//stop blinking active step led
	led_setBlinkLed(selectedStepLed, 0);
	led_setValue(0, selectedStepLed);

	//reset select leds
	//led_clearSelectLeds();

//	menu_switchSubPage(lastActiveSubPage);
//	menu_switchPage(lastActivePage);
}*/
//--------------------------------------------------------
/*
// when in voice mode you press shift, you enter sequencer mode
static void buttonHandler_enterSeqMode() {
	lastActivePage = menu_activePage;
	lastActiveSubPage = menu_getSubPage();
	menu_switchSubPage(0, 1);
	menu_switchPage(SEQ_PAGE);

	buttonHandler_updateSubSteps();

	led_clearSelectLeds();
	//led_setBlinkLed(selectedStepLed, 1);
}
*/
//--------------------------------------------------------
/*
static void buttonHandler_leaveSeqMode() {
	//stop blinking active step led
	led_setBlinkLed(selectedStepLed, 0);
	led_setValue(0, selectedStepLed);

	//reset select leds
	led_clearSelectLeds();

	menu_switchPage(lastActivePage);
	menu_switchSubPage(lastActiveSubPage, 1);

}
*/
//--------------------------------------------------------
//uint8_t buttonHandler_getMutedVoices() {
//	return buttonHandler_mutedVoices;
//}
//--------------------------------------------------------
static void buttonHandler_selectActiveStep(uint8_t ledNr, uint8_t seqButtonPressed) {

	led_setBlinkLed(selectedStepLed, 0);
	led_setValue(0, selectedStepLed);

	if (buttonHandler_selectedStep == (seqButtonPressed)) {
		menu_switchSubPage(menu_getSubPage(), 1); //to enable toggle
	}

	//update active step
	buttonHandler_selectedStep = (uint8_t) (seqButtonPressed);
	selectedStepLed = ledNr;

	activeParameters[PAR_ACTIVE_STEP] = buttonHandler_selectedStep + menu_activeBar*16;

	//blink new step
	led_setBlinkLed(ledNr, 1);

	//update sub steps
	//request step parameters from sequencer 
	frontPanel_sendData(SEQ_CC, SEQ_REQUEST_STEP_PARAMS,
			(uint8_t) (seqButtonPressed+ menu_activeBar*16 ));

	//update sub steps
	buttonHandler_updateSubSteps();
}
//--------------------------------------------------------
static void buttonHandler_setRemoveStep(uint8_t ledNr, uint8_t seqButtonPressed) {
	//led_toggle(ledNr); //handled by cortex
	led_setValue(0, ledNr);

#if 0
	//we have 128 steps, the main buttons are only for multiples of 8
	DISABLE_CONV_WARNING
	//we have no more substeps so we remove *8
	seqButtonPressed *= 8;
	END_DISABLE_CONV_WARNING
#endif

	//update active step (so that seq mode always shows the last set step)
	buttonHandler_selectedStep = seqButtonPressed;
	activeParameters[PAR_ACTIVE_STEP] = buttonHandler_selectedStep + menu_activeBar*16;
	selectedStepLed = ledNr;

	//request step parameters from sequencer
	frontPanel_sendData(SEQ_CC, SEQ_REQUEST_STEP_PARAMS, seqButtonPressed+ menu_activeBar*16);

	//set sequencer step on soundchip
	uint8_t trackNr = menu_getActiveVoice(); //max 6 => 0x6 = 0b110
	uint8_t patternNr = menu_getViewedPattern(); //max 7 => 0x07 = 0b111
#if 0
	uint8_t value = (uint8_t) ((trackNr << 4) | (patternNr & 0x7));

	frontPanel_sendData(MAIN_STEP_CC, value, seqButtonPressed / 8);
#else
	//frontPanel_sendData(MAIN_STEP_CC, value, seqButtonPressed);
	seq_toggleMainStep(trackNr, seqButtonPressed + 16*menu_activeBar, patternNr);
	if(seq_isMainStepActive(trackNr, seqButtonPressed + 16*menu_activeBar, patternNr))
	{
		led_setValue(1,led_idx2StepLed(seqButtonPressed));
	}
#endif
	//autosave changes
	preset_touchTimer(PRESET_PAT_TIMER);

}

//--------------------------------------------------------
// **PATROT set track rotation to a value between 0 and 15
// 0 means not rotated.
// **PATROT todo need to update seq leds with current rotation for this track
/*
static void buttonHandler_setTrackRotation(uint8_t seqButtonPressed)
{
	parameter_values[PAR_TRACK_ROTATION]=seqButtonPressed;
	frontPanel_sendData(SEQ_CC, SEQ_TRACK_ROTATION, seqButtonPressed);
	// update the value right now (this is also updated in the code that handles the shift button press)
	led_clearAllBlinkLeds();
	led_setBlinkLed(led_idx2StepLed(seqButtonPressed), 1);
}
*/
//--------------------------------------------------------
static void buttonHandler_seqButtonPressed(uint8_t seqButtonPressed)
{


	uint8_t ledNr = led_idx2StepLed(seqButtonPressed);

	if (buttonHandler_getShift()) {

		switch(seqButtonPressed)
		{

		case SEQ_DRUM1:
		case SEQ_DRUM2:
		case SEQ_DRUM3:
		case SEQ_SNARE:
		case SEQ_CLP:
		case SEQ_CL_HH:
		case SEQ_OP_HH:
			//single instrument buttons
			break;

		case SEQ_KIT: //kit: engine menu of the active voice
			engineMenu_open();
			break;

		case SEQ_MORPH: //morph
#if CPU_METER
			//development builds: simulate more FM voices (0..4), restart the peak measurement
			//(on the timing and stall pages: only restart the measurement)
			if(menu_cpuMeterPage() < 3)
				cpuMeter.stressFm = (cpuMeter.stressFm + 1) % (CPU_METER_MAX_STRESS + 1);
			cpuMeter_resetPeaks();
#endif
			break;

		case SEQ_FX:
			buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_FX;
			led_clearAllBlinkLeds();
			led_setBlinkLed(LED_STEP15,1);
			menu_switchPage(FX_MODE_PAGE);
			menu_switchSubPage(fxBus.fxType, 1 );


			//change visible parameters to selected FX type
			if(menu_getSubPage() != fxBus.fxType)
				menu_switchSubPage(fxBus.fxType, 1);
			break;

		case SEQ_SONG:
			//--> enter song mode
			buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_SONG;
			menu_switchPage(SONG_MODE_PAGE);
			break;
		case SEQ_PATTERN: //Pattern

			//--> pattern select mode
			buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_PATTERN;
			menu_switchPage(PATTERN_LOAD_PAGE);

			break;
		case SEQ_PROJECT: //Performance
#if CPU_METER
			//development builds: show/hide the CPU load page
			menu_toggleCpuMeter();
#endif
			break;

		case SEQ_RELOAD:
		{
			//if in LAST STEP mode, reload pattern to reset pattern and track lengths
		//	uint8_t mode = buttonHandler_getMode();

			//if((mode == SELECT_MODE_LAST_STEP_TRACK) || (mode == SELECT_MODE_LAST_STEP_ALL))
			//{

				//tell sequencer to change/reload current pattern
				frontPanel_sendData(SEQ_CC, SEQ_CHANGE_PAT, seq_activePattern);

			//}
			//else
			//{
				//reload kit to edit buffer
				preset_loadDrumsetRam(menu_loadedPresetNr[CURRENT_KIT],0, 0);
				preset_touchTimer(PRESET_KIT_TIMER);
			//}
		}
			break;

		case SEQ_CONFIG: //config
			//(was SEQ_CONFIG, a step button number equal to SELECT_MODE_LAST_STEP_ALL:
			//the next step press set the length of all tracks)
			buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_MENU;
			led_clearAllBlinkLeds();
			led_setBlinkLed(LED_STEP12,1);
			menu_switchPage(MENU_MIDI_PAGE);
			break;


		case SEQ_LAST_STEP:

			//the blinking led is only

			led_clearAllBlinkLeds();
			const uint8_t len = seq_getTrackLength(menu_activeVoice)-1;
			//check if blinking LED is on active page/bar
			const uint8_t isOnActivePage = (len >= menu_activeBar*16) && (len < (menu_activeBar*16+16));
			if(isOnActivePage)
			{
				led_setBlinkLed(led_idx2StepLed(len%16),1); //the current last step should be visible
			}
			if(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_LAST_STEP_TRACK) buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_LAST_STEP_ALL;
			//else if(buttonHandler_stateMemory.selectButtonMode == SEQ_LAST_STEP_ALL) buttonHandler_stateMemory.selectButtonMode = SEQ_LAST_STEP_SINGLE_TRACK;
			else buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_LAST_STEP_TRACK;
			menu_switchPage(MENU_LAST_STEP_PAGE);


		default:
			break;
		}

		/*
		switch(buttonHandler_stateMemory.selectButtonMode) {
		case SELECT_MODE_VOICE:
			//buttonHandler_selectActiveStep(ledNr, seqButtonPressed );
			break;
		case SELECT_MODE_STEP: // step edit mode -> adds/removes step because shift is down
			//buttonHandler_setRemoveStep(ledNr, seqButtonPressed);
			break;
		case SELECT_MODE_PERF: // **PATROT shift is held while in perf mode and a seq button is pressed
			//buttonHandler_setTrackRotation(seqButtonPressed);
			break;
		default:
			break;
		}
		*/
	}
	else if(buttonHandler_getLoad())
	{
		switch(seqButtonPressed)
		{

			case SEQ_DRUM1:
			case SEQ_DRUM2:
			case SEQ_DRUM3:
			case SEQ_SNARE:
			case SEQ_CLP:
			case SEQ_CL_HH:
			case SEQ_OP_HH:
				//single instrument buttons: load the voice from another kit
				voiceLoad_open(seqButtonPressed);
				break;

			case SEQ_KIT: //kit
				menu_setLoadSaveCategory(SAVE_TYPE_KIT);
				//update the status LED
				led_setMode2(2);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP8,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_LOAD;
				menu_switchPage(LOAD_PAGE);
				break;
			case SEQ_MORPH: //morph
				menu_setLoadSaveCategory(SAVE_TYPE_MORPH);
				//update the status LED
				led_setMode2(2);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP9,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_LOAD;
				menu_switchPage(LOAD_PAGE);
				break;

			case SEQ_PATTERN: //Pattern

#if 1
				//--> pattern select mode
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_PATTERN;
				menu_switchPage(PATTERN_LOAD_PAGE);
#else
				//update the status LED
				led_setMode2(2);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP16,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_LOAD;
				menu_switchPage(LOAD_PAGE);
				menu_setLoadSaveCategory(SAVE_TYPE_PATTERN);
#endif
				break;

			case SEQ_SONG:
				menu_setLoadSaveCategory(SAVE_TYPE_SONG);
				//update the status LED
				led_setMode2(2);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP10,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_LOAD;
				menu_switchPage(LOAD_PAGE);
				break;
			case SEQ_PROJECT: //Performance

				menu_setLoadSaveCategory(SAVE_TYPE_PROJECT);
				//update the status LED
				led_setMode2(2);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP11,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_LOAD;
				menu_switchPage(LOAD_PAGE);
				break;

			case SEQ_CONFIG: //config

				break;

		//	case SEQ_PATTERN: //Pattern
				//--> pattern select mode
				//buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_PATTERN;
				//menu_switchPage(PATTERN_LOAD_PAGE);
			//	break;

			default:
				break;
		}
	}
	else if(buttonHandler_getSave())
	{
		switch(seqButtonPressed)
		{

			case SEQ_DRUM1:
			case SEQ_DRUM2:
			case SEQ_DRUM3:
			case SEQ_SNARE:
			case SEQ_CLP:
			case SEQ_CL_HH:
			case SEQ_OP_HH:
				//single instrument buttons
				break;

			case SEQ_KIT: //kit
				menu_setLoadSaveCategory(SAVE_TYPE_KIT);
				//update the status LED
				led_setMode2(3);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP8,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_SAVE;
				menu_switchPage(SAVE_PAGE);
				break;
			case SEQ_MORPH: //morph
				menu_setLoadSaveCategory(SAVE_TYPE_MORPH);
				//update the status LED
				led_setMode2(3);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP9,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_SAVE;
				menu_switchPage(SAVE_PAGE);
				break;
/*
			case SEQ_PATTERN: //Pattern
				//--> pattern select mode
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_PATTERN;
				menu_switchPage(PATTERN_SAVE_PAGE);
				break;
				*/

			case SEQ_PATTERN: //Pattern



				//update the status LED
				led_setMode2(3);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP16,1);
				//buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_SAVE;
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_PATTERN;
				menu_switchPage(PATTERN_SAVE_PAGE);
				menu_setLoadSaveCategory(SAVE_TYPE_PATTERN);
				break;


			case SEQ_SONG:
				menu_setLoadSaveCategory(SAVE_TYPE_SONG);
				//update the status LED
				led_setMode2(3);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP10,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_SAVE;
				menu_switchPage(SAVE_PAGE);
				break;
			case SEQ_PROJECT: //Performance
				menu_setLoadSaveCategory(SAVE_TYPE_PROJECT);
				//update the status LED
				led_setMode2(3);
				led_clearAllBlinkLeds();
				led_setBlinkLed(LED_STEP11,1);
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_SAVE;
				menu_switchPage(SAVE_PAGE);
				break;

			case SEQ_CONFIG: //config

				break;

			default:
				break;
		}
	}
	else { // no key modifier pressed
		switch (buttonHandler_stateMemory.selectButtonMode) {
		case SELECT_MODE_VOICE:
			// button is held down, start a timer. If the button is held we
			// can record automation for the one button. The regular handling for
			// adding/removing step is handled in the button release
			//buttonHandler_setTimeraction((uint8_t) (seqButtonPressed * 8));
			buttonHandler_setTimeraction((uint8_t) (seqButtonPressed + menu_activeBar*16));
			break;
		case SELECT_MODE_STEP:
			led_clearAllBlinkLeds();
			buttonHandler_selectActiveStep(ledNr, seqButtonPressed);

			//reset sub step to 1 (== main step parameters)
			//led_setBlinkLed(LED_PART_SELECT1, 1);

			//set Mode button blinking again
			led_setBlinkLed(LED_MODE1, 1);
			break;

		case SELECT_MODE_PATTERN:
			if(menu_getActivePage() == PATTERN_SAVE_PAGE)
			{


				//write pattern edit buffer back to RAM pattern array
			//	seq_copyEditBufferToRam(seqButtonPressed + 16 * menu_activePatternBank);


				if(activeParameters[PAR_PATTERN_MODE] == 1)
				{
					//prevent saving to a new slot reloading the kit and discarding unsaved changes
					activeParameters[PAR_PATTERN_MODE] = SKIP_NEXT_KIT_AUTOLOAD;
				}

				seq_patternSet.shuffle[seqButtonPressed + 16 * menu_activePatternBank] = activeParameters[PAR_SHUFFLE];



				preset_savePatternRAM(seqButtonPressed + 16 * menu_activePatternBank);

				//save to tmp folder
				#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
					HAL_TIM_Base_Start_IT(&htim2);	//calc sample interrupt - prio 1
				#else

				#endif
					preset_savePatternSD(seqButtonPressed + 16 * menu_activePatternBank);
				#if USE_INTERRUPT_ONLY_DURING_SAVE_PROJ
							HAL_TIM_Base_Stop_IT(&htim2);	//calc sample interrupt - prio 1
				#else

				#endif


				menu_selectedPresetNr[SAVE_TYPE_PATTERN] = seqButtonPressed + 16 * menu_activePatternBank;
				menu_activePage = PATTERN_SAVE_PAGE_OK;
				sucessTimer = systick_ticks;
				menu_repaint();

			}
			else
			{
				//a new pattern is manually selected -> KCM=auto should load in every case
				//SKIP_NEXT_KIT_AUTOLOAD is just to prevent autoload after saving to a new slot
				if(activeParameters[PAR_PATTERN_MODE] == SKIP_NEXT_KIT_AUTOLOAD)
				{
					activeParameters[PAR_PATTERN_MODE] = 1;
				}
				//select another pattern to play -> copy to edit buffer


				//select pattern to show // shownPattern
				menu_setShownPattern(seqButtonPressed + 16 * menu_activePatternBank);
				led_clearSequencerLeds();

				led_clearAllBlinkLeds();
				led_setBlinkLed(led_idx2StepLed(seqButtonPressed), 1);

				frontPanel_sendData(SEQ_CC, SEQ_REQUEST_EUKLID_PARAMS,
						menu_activePage/*menu_getActiveVoice()*/);
				//update autosave pattern on manual pattern change
				preset_touchTimer(PRESET_PAT_TIMER);

	//-----------------------------------------------------------------


				//flash corresponding LED until ACK (SEQ_CHANGE_PAT) received
				ledNr = led_idx2StepLed(seqButtonPressed);
				led_clearAllBlinkLeds();

				led_setBlinkLed(ledNr, 1);

				//tell sequencer to change pattern
				frontPanel_sendData(SEQ_CC, SEQ_CHANGE_PAT, seqButtonPressed + 16 * menu_activePatternBank);
			}
			break;


		case SELECT_MODE_PERF: //--- buttons 1-8 initiate a manual roll
			if (seqButtonPressed < 8) {
				//turn roll on
				frontPanel_sendData(SEQ_CC, SEQ_ROLL_ON_OFF,
						(uint8_t) ((seqButtonPressed & 0xf) + 0x10));
				//turn button led on
				led_setValue(1, ledNr);
			} else {
				/*
				 //moved pattern select to select buttons
				 //pattern

				 //tell sequencer to change pattern
				 frontPanel_sendData(SEQ_CC,SEQ_CHANGE_PAT,seqButtonPressed-8);
				 //flash corresponding LED until ACK (SEQ_CHANGE_PAT) received
				 led_setBlinkLed(ledNr,1);

				 //request the pattern info for the selected pattern (bar cnt, next...)
				 frontPanel_sendData(SEQ_CC,SEQ_REQUEST_PATTERN_PARAMS,seqButtonPressed-8);
				 */
			}
			break;

		case SELECT_MODE_SONG:
			//in song mode, seq buttons add patterns to song
			seq_addPatternToSong(seqButtonPressed + 16 * menu_activePatternBank);
			menu_repaintAll();
			break;

		case SELECT_MODE_LAST_STEP_ALL:
			{
				const uint8_t barOffset = menu_activeBar*16;
				for(uint8_t i = 0;i<7;i++)
				{
					seq_setTrackLength(i, seqButtonPressed+barOffset+1);
				}
				menu_repaint();
				led_clearAllBlinkLeds();

				const uint8_t len = seq_getTrackLength(menu_activeVoice)-1;
				//check if blinking LED is on active page/bar
				const uint8_t isOnActivePage = (len >= menu_activeBar*16) && (len < (menu_activeBar*16+16));
				if(isOnActivePage)
				{
					led_setBlinkLed(led_idx2StepLed(len%16),1); //the current last step should be visible
				}
			}
			break;

		case SELECT_MODE_LAST_STEP_TRACK:
			{
				const uint8_t barOffset = menu_activeBar*16;
				seq_setTrackLength(menu_activeVoice, seqButtonPressed+barOffset+1);
				menu_repaint();
				led_clearAllBlinkLeds();

				const uint8_t len = seq_getTrackLength(menu_activeVoice)-1;
				//check if blinking LED is on active page/bar
				const uint8_t isOnActivePage = (len >= menu_activeBar*16) && (len < (menu_activeBar*16+16));
				if(isOnActivePage)
				{
					led_setBlinkLed(led_idx2StepLed(len%16),1); //the current last step should be visible
				}
			}
			break;

			//--- unused (maybe lfo clock sync? ---
		case SELECT_MODE_LOAD:
		case SELECT_MODE_SAVE:
		default:
			break;
		}
	} // is shift being held down
}
//--------------------------------------------------------
static void buttonHandler_seqButtonReleased(uint8_t seqButtonPressed)
{

	uint8_t ledNr = led_idx2StepLed(seqButtonPressed);

	if(buttonHandler_stateMemory.selectButtonMode == SEQ_LAST_STEP)
	{

	}
	if (buttonHandler_getShift()) // shift click is fully handled in "pressed"
	{

		switch (buttonHandler_stateMemory.selectButtonMode)
		{
		default:
			break;

			//always check seq buttons to turn off any active rolls
		case SELECT_MODE_PERF:
			if (seqButtonPressed < 8) {
				//turn roll off
				frontPanel_sendData(SEQ_CC, SEQ_ROLL_ON_OFF, (seqButtonPressed & 0xf));
				//turn button led off
				led_setValue(0, ledNr);
			}
			break;

		}
		return;
	}
	else if(buttonHandler_getLoad() || buttonHandler_getSave()) return;

	// I don't think that the timer would be armed in this case
	//{
		//do nothing if timer action occured
		//if (buttonHandler_TimerActionOccured())
			//return;

		//if ((buttonHandler_getMode() == SELECT_MODE_VOICE)) {
			//sequencer mode -> buttons select active step
			//buttonHandler_selectActiveStep(ledNr, seqButtonPressed);
		//} else if ((buttonHandler_getMode() == SELECT_MODE_STEP)) {
			//buttonHandler_setRemoveStep(ledNr,seqButtonPressed);
		//}

	//} else {

	switch (buttonHandler_stateMemory.selectButtonMode) {
	case SELECT_MODE_STEP:
		//do nothing if timer action occurred - its already handled in the timer
		if (buttonHandler_TimerActionOccured())
			return;
		break;
		//--- edit the pattern -> button sets and removes a step ---
	case SELECT_MODE_VOICE:

		//do nothing if timer action occured
		if (buttonHandler_TimerActionOccured())
			return;
		buttonHandler_setRemoveStep(ledNr, seqButtonPressed);

		break;
	case SELECT_MODE_PERF:
		if (seqButtonPressed < 8) {
			//turn roll off
			frontPanel_sendData(SEQ_CC, SEQ_ROLL_ON_OFF, (seqButtonPressed & 0xf));
			//turn button led off
			led_setValue(0, ledNr);
		}
		break;

		//--- unused (maybe lfo clock sync? ---
	case SELECT_MODE_LOAD:
		break;



	default:
		break;

	}
}

//--------------------------------------------------------
// one of the 8 part buttons is pressed
static void buttonHandler_selectButtonPressed(uint8_t partNr)
{
	//if in step mode, return to voice/edit mode
	if(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_STEP)
	{
				led_clearAllBlinkLeds();
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_VOICE;
				//update the status LED
				led_setMode2(SELECT_MODE_VOICE);
				//set menu to voice page mode
				menu_switchPage(menu_getActiveVoice());
				led_setActiveSelectButton(menu_getSubPage());
				menu_resetActiveParameter();
	}
	//-----
/*
 * comment removes copy pattern function, can now be achieved with save pattern as when edit buffer is implemented for patterns
	if (copyClear_Mode >= MODE_COPY_PATTERN) {
		//copy mode
		if (copyClear_srcSet()) { // if we have already selected a source, do the copy operation
			//select dest
			copyClear_setDst((int8_t)partNr, MODE_COPY_PATTERN);
			copyClear_copyPattern();
			led_clearAllBlinkLeds();

			//query current sequencer step states and light up the corresponding leds
#if 0
			uint8_t trackNr = menu_getActiveVoice(); //max 6 => 0x6 = 0b110
			uint8_t patternNr = menu_getViewedPattern(); //max 7 => 0x07 = 0b111
			uint8_t value = (uint8_t) ((trackNr << 4) | (patternNr & 0x7));
			frontPanel_sendData(LED_CC, LED_QUERY_SEQ_TRACK, value);
#else
			led_updateSeqLeds();
#endif
		} else {
			//no source selected, so select src
			copyClear_setSrc((int8_t)partNr, MODE_COPY_PATTERN);
			led_setBlinkLed(led_idx2SelectLed(partNr), 1);
		}
	} else */ { // none or clear
		// the code that handles most of this case is in the button release
		// if shift is held and a part button is held down, we will enter automation record mode
		// for that sub-step on the currently selected step.
		// if shift is not held, we just do normal handling
		//(SHIFT + held select button armed step selectedStep*8+partNr for automation:
		//the v1 sub step formula, up to 511 -> recorded far past the 64 steps of a track)
		buttonHandler_handleSelectButton(partNr);

	}

}

//--------------------------------------------------------
// one of the 8 part buttons is released
static void buttonHandler_partButtonReleased(uint8_t partNr)
{
	if (copyClear_Mode >= MODE_COPY_PATTERN) {

	} else {
		//if(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_VOICE) return;
		//do nothing if timer action occured
		if (buttonHandler_TimerActionOccured())
			return;

		buttonHandler_buttonTimerStepNr = NO_STEP_SELECTED;
		if (buttonHandler_getShift() && buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_VOICE) {
			buttonHandler_handleSelectButton(partNr);
		}
	}
}

//--------------------------------------------------------
// one of the 7 voice buttons pressed
static void buttonHandler_voiceButtonPressed(uint8_t voiceNr)
{

	//if in step mode, return to voice/edit mode
	if(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_STEP)
	{
				led_clearAllBlinkLeds();
				buttonHandler_stateMemory.selectButtonMode = SELECT_MODE_VOICE;
				//update the status LED
				led_setMode2(SELECT_MODE_VOICE);
				//set menu to voice page mode
				menu_switchPage(menu_getActiveVoice());
				led_setActiveSelectButton(menu_getSubPage());
				menu_resetActiveParameter();
	}

	if (copyClear_Mode >= MODE_COPY_PATTERN) {
		//copy mode -> voice buttons select track copy src/dst
		if (copyClear_srcSet()) {
			//select dest
			copyClear_setDst((int8_t)voiceNr, MODE_COPY_TRACK);
			copyClear_copyTrack();
			led_clearAllBlinkLeds();
			//query current sequencer step states and light up the corresponding leds
#if 0
			uint8_t trackNr = menu_getActiveVoice(); //max 6 => 0x6 = 0b110
			uint8_t patternNr = menu_getViewedPattern(); //max 7 => 0x07 = 0b111
			uint8_t value = (uint8_t) ((trackNr << 4) | (patternNr & 0x7));
			frontPanel_sendData(LED_CC, LED_QUERY_SEQ_TRACK, value);
#else
			led_updateSeqLeds();
#endif

		} else {
			// no source selected so select src
			copyClear_setSrc((int8_t)voiceNr, MODE_COPY_TRACK);
			led_setBlinkLed((uint8_t) (LED_VOICE1 + voiceNr), 1);
		}
	} else {
		// mode none or clear
		// determine if we are in mute/unmute mode -
		// in perf mode we are in mute mode unless shift is held
		// in any other mode we are in mute mode when shift is held
		uint8_t muteModeActive = buttonHandler_getShift();
		if (menu_activePage == PERF_PAGE) {//(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_PERF) {
			//invert mute functionality
			muteModeActive = (uint8_t) (1 - muteModeActive);
		}

		if (muteModeActive) {
			//un/mute
			if (buttonHandler_mutedVoices & (1 << voiceNr)) {
				//unmute tracks 0-7
				buttonHandler_muteVoice(voiceNr, 0);
				frontPanel_sendData(SEQ_CC, SEQ_UNMUTE_TRACK, voiceNr);
			} else {
				//mute tracks 0-7
				buttonHandler_muteVoice(voiceNr, 1);
				frontPanel_sendData(SEQ_CC, SEQ_MUTE_TRACK, voiceNr);
			}

		}
		//############ mute mode is not active. regular handling ############
		else {
			if (menu_activePage == PERF_PAGE) {//(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_PERF) {
				//shift-voice in perf mode -> unmute all tracks up to and including selected voice which are muted
				for (uint8_t i = 0; i <= voiceNr; i++) {
					if (buttonHandler_mutedVoices & (1 << i)) {
						frontPanel_sendData(SEQ_CC, SEQ_UNMUTE_TRACK, i);
						DISABLE_CONV_WARNING
						buttonHandler_mutedVoices &= ~(1<<i);
						END_DISABLE_CONV_WARNING
					}
				}
				buttonHandler_showMuteLEDs();

			} else { //select active voice

				//check active bars of new track and make sure the view does not end up in inactive part
				uint8_t activeStep = seq_getStepIndex(voiceNr);
				if(activeStep == 255)
				{
					menu_activeBar = 0;
				}
				else
				{
					menu_activeBar = activeStep / 16;
				}
				//we also update the bar/page buttons here
				led_updateBarLeds();


				//the currently active button is lit
				led_setActiveVoice(voiceNr);

				//change voice page on display if in voice mode
				if ((buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_VOICE) || (buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_PERF) /*|| (buttonHandler_stateMemory.selectButtonMode ==SELECT_MODE_STEP) */) {
					menu_switchPage(voiceNr);
				}
				frontPanel_sendData(SEQ_CC, SEQ_SET_ACTIVE_TRACK, voiceNr);

				menu_setActiveVoice(voiceNr);

				frontPanel_sendData(SEQ_CC, SEQ_REQUEST_EUKLID_PARAMS, voiceNr);
				//request the pattern info for the selected pattern (bar cnt, next...)
				//frontPanel_sendData(SEQ_CC,SEQ_REQUEST_PATTERN_PARAMS,buttonNr);

				led_updateSeqLeds();

				if (buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_STEP) {
					//reactivate sequencer mode
					led_clearAllBlinkLeds();
					buttonHandler_enterSeqModeStepMode();

				}
				else if (buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_ENGINE) {
					//engine menu of the new voice
					engineMenu_open();
				}
				else if (menu_activePage == VOICE_LOAD_PAGE) {
					//load menu of the new voice
					voiceLoad_open(voiceNr);
				}





			} // if buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_PERF)
		} // if (muteModeActive)
	} // if (copyClear_Mode >= MODE_COPY_PATTERN)


}

//--------------------------------------------------------
void buttonHandler_buttonPressed(uint8_t buttonNr) {

	screensaver_touch();


	//------------------------------------------------------
	switch(buttonNr)
	{


	case BUT_VOICE_1:
	case BUT_VOICE_2:
	case BUT_VOICE_3:
	case BUT_VOICE_4:
	case BUT_VOICE_5:
	case BUT_VOICE_6:
	case BUT_VOICE_7:
		buttonHandler_voiceButtonPressed(buttonNr-BUT_VOICE_1);
		break;

	case BUT_PAGE_UP:
		if((buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_PATTERN) || (buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_SONG))
		{
			//pattern bank mode
			if(menu_activePatternBank < 3)
			{
				//save current pattern set
				//preset_savePatternTemp(menu_activePatternBank);

				menu_activePatternBank++;

				//load new pattern set in background
				//preset_loadPatternTemp(menu_activePatternBank);
				led_initPatternSelectLeds();
			}
		}
		else
		{
			//step view -> bar mode
			if(menu_activeBar < NUM_BARS-1)
			{
				menu_activeBar++;

				//if copy button is pressed, fill next bar with current bar + extend track length to next full multiple of 16
				if(buttonHandler_getCopy())
				{
					//extend length according to BAR
					seq_setTrackLength(menu_getActiveVoice(), (menu_activeBar+1) *16);

					Step *src, *dst;
					for(int k=0;k<16;k++)
					{
						dst=&seq_patternSet.seq_subStepPattern[seq_activePattern][menu_getActiveVoice()][(menu_activeBar*16)+k];
						src=&seq_patternSet.seq_subStepPattern[seq_activePattern][menu_getActiveVoice()][((menu_activeBar-1)*16)+k];
						dst->note		= src->note;
						dst->param1Nr 	= src->param1Nr;
						dst->param1Val 	= src->param1Val;
						dst->param2Nr	= src->param2Nr;
						dst->param2Val	= src->param2Val;
						dst->prob		= src->prob;
						dst->volume		= src->volume;
					}

					/*
					 * 	Step seq_subStepPattern[NUM_PATTERN][NUM_TRACKS][NUM_STEPS];	//contains step data
						uint16_t seq_mainSteps[NUM_BARS][NUM_PATTERN][NUM_TRACKS];		//bits represent if step is active
					 */
					//for(int i=0;i<NUM_TRACKS;i++)
					//{
						seq_patternSet.seq_mainSteps[menu_activeBar][seq_activePattern][menu_getActiveVoice()] = seq_patternSet.seq_mainSteps[menu_activeBar - 1][seq_activePattern][menu_getActiveVoice()];
					//}
				}

				//if shift is pressed, set pattern length to end of next bar
				if(buttonHandler_getShift())
				{
					const uint8_t len = 16 + 16* menu_activeBar;
					seq_setTrackLength(menu_getActiveVoice(), len);
							//menu_repaint();
							//led_clearAllBlinkLeds();
				}
			}
			//refresh LEDs
			if((buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_LAST_STEP_TRACK) ||(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_LAST_STEP_ALL))
			{
				led_clearAllBlinkLeds();
				const uint8_t len = seq_getTrackLength(menu_activeVoice)-1;
				const uint8_t isOnActivePage = (len >= menu_activeBar*16) && (len < (menu_activeBar*16+16));
				if(isOnActivePage)
				{
					led_setBlinkLed(led_idx2StepLed(len%16),1); //the current last step should be visible
				}

				//we also update the bar/page buttons here
				led_updateBarLeds();
			}
			else
			{
				led_updateSeqLeds();
			}
		}
		break;

	case BUT_PAGE_DOWN:
		if((buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_PATTERN) || (buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_SONG))
		{
			//pattern bank mode
			if(menu_activePatternBank > 0)
			{
				//save current pattern set
				//preset_savePatternTemp(menu_activePatternBank);

				menu_activePatternBank--;
				//load new pattern bank in background
				//preset_loadPatternTemp(menu_activePatternBank);
				led_initPatternSelectLeds();


			}
		}
		else
		{
			if(menu_activeBar > 0)
			{
				//seq bar mode
				menu_activeBar--;

				//if shift is pressed, set pattern length to end of next bar
				if(buttonHandler_getShift())
				{
					const uint8_t len = 16 + 16* menu_activeBar;
					seq_setTrackLength(menu_getActiveVoice(), len);
							//menu_repaint();
							//led_clearAllBlinkLeds();
				}
			}

			if((buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_LAST_STEP_TRACK) ||(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_LAST_STEP_ALL))
			{
				led_clearAllBlinkLeds();
				const uint8_t len = seq_getTrackLength(menu_activeVoice)-1;
				const uint8_t isOnActivePage = (len >= menu_activeBar*16) && (len < (menu_activeBar*16+16));
				if(isOnActivePage)
				{
					led_setBlinkLed(led_idx2StepLed(len%16),1); //the current last step should be visible
				}

				//we also update the bar/page buttons here
				led_updateBarLeds();
			}
			else
			{
				led_updateSeqLeds();
			}
		}
		break;

	case BUT_SELECT5:
		buttonHandler_selectButtonPressed(4);
		break;
	case BUT_SELECT6:
		buttonHandler_selectButtonPressed(5);
		break;
	case BUT_SELECT7:
		buttonHandler_selectButtonPressed(6);
		break;
	case BUT_SELECT8:
		buttonHandler_selectButtonPressed(7);
		break;

	case BUT_SEQ16:
		buttonHandler_seqButtonPressed(15);
		break;

	case BUT_SEQ15:
		buttonHandler_seqButtonPressed(14);
		break;

	case BUT_SEQ14:
		buttonHandler_seqButtonPressed(13);
		break;

	case BUT_SEQ13:
		buttonHandler_seqButtonPressed(12);
		break;


	case BUT_SELECT1:
		buttonHandler_selectButtonPressed(0);
		break;
	case BUT_SELECT2:
		buttonHandler_selectButtonPressed(1);
		break;
	case BUT_SELECT3:
		buttonHandler_selectButtonPressed(2);
		break;
	case BUT_SELECT4:
		buttonHandler_selectButtonPressed(3);
		break;

		break;

	case BUT_SEQ12:
		buttonHandler_seqButtonPressed(11);
		break;

	case BUT_SEQ11:
		buttonHandler_seqButtonPressed(10);
		break;

	case BUT_SEQ10:
		buttonHandler_seqButtonPressed(9);
		break;

	case BUT_SEQ9:
		buttonHandler_seqButtonPressed(8);
		break;


	case BUT_MODE1 :
	case BUT_MODE2 :
	case BUT_MODE3 :
	case BUT_MODE4 :
		buttonHandler_handleModeButtons((uint8_t) (buttonNr - BUT_MODE1));
		break;

	case BUT_SEQ8:
		buttonHandler_seqButtonPressed(7);
		break;

	case BUT_SEQ7:
		buttonHandler_seqButtonPressed(6);
		break;

	case BUT_SEQ6:
		buttonHandler_seqButtonPressed(5);
		break;

	case BUT_SEQ5:
		buttonHandler_seqButtonPressed(4);
		break;


	case BUT_SHIFT:
			/* while this button is pressed, the SEQUENCER mode is activated
			 -> change display to show selected step options (volume, probability, note etc...)
			 -> seq buttons select step
			 -> selected step is illuminated on the seq buttons
			 -> select buttons show 8 sub steps for the selected step

			 ->shows muted steps on voice leds while pressed
			 */

			led_setValue(1, LED_SHIFT);
			switch(buttonHandler_stateMemory.selectButtonMode) {
				case SELECT_MODE_VOICE:
					//step parameter function not available via shift anymore.
					//shift is only used for pattern, song cfg etc with the seq buttons...
					//step parameters only via step mode (2 x voice mode)

					break;
				case SELECT_MODE_PERF:
				case SELECT_MODE_PAT_GEN:

					// Pattern changes will be done via SHIFT+PATTERN button, so do nothing here in V2

					/*
					//blink selected pattern LED
					menu_switchPage(PATTERN_SETTINGS_PAGE);
					led_clearSelectLeds();
					led_clearAllBlinkLeds();

					if (buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_PAT_GEN)
						led_setBlinkLed(LED_MODE2, 1);
					else {
						// **PATROT The step led's are repurposed here to have the current track rotation
						// represented by a blinking led. The leftmost LED blinking means rotation is off (0)
						// and each one represents a different rotation
						led_setBlinkLed(led_idx2StepLed(parameter_values[PAR_TRACK_ROTATION]), 1);
					}

		// todo taking the below out while in perf mode so I can see if the above works. Not sure if the below is needed, since
		// at this stage the shift button is held
					//the pattern change update for the follow mode is not made immediately when the pattern options are active
					//so we have to do it here
					if(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_PAT_GEN) {
					if (parameter_values[PAR_FOLLOW]) {
						menu_setShownPattern(menu_shownPattern);
						led_clearSequencerLeds();
#if 0
						//query current sequencer step states and light up the corresponding leds
						uint8_t trackNr = menu_getActiveVoice(); //max 6 => 0x6 = 0b110
						uint8_t patternNr = menu_getViewedPattern(); //max 7 => 0x07 = 0b111
						uint8_t value = (uint8_t) ((trackNr << 4) | (patternNr & 0x7));
						frontPanel_sendData(LED_CC, LED_QUERY_SEQ_TRACK, value);
#else
						led_updateSeqLeds();
#endif
						frontPanel_sendData(SEQ_CC, SEQ_REQUEST_PATTERN_PARAMS,
								frontParser_midiMsg.data2);
					}
					}

					led_setBlinkLed(led_idx2SelectLed(menu_getViewedPattern()),	1);
					*/
					break;

				case SELECT_MODE_STEP:
					//buttonHandler_leaveSeqModeStepMode();
					break;
				default:
					break;
			}

			//show muted voices if pressed
			buttonHandler_showMuteLEDs();

		break;

	case BUT_START_STOP:
		if(buttonHandler_getShift())
		{
			seq_setSongModeEnabled(1);
		}
		else
		{
			seq_setSongModeEnabled(0);
		}

		//only use start button if external sync is deactivated
		//if(seq_getExtSync() == 0)
		{
			buttonHandler_setRunStopState(
					(uint8_t) (1 - buttonHandler_stateMemory.seqRunning));
			//send run/stop command to soundchip
			frontPanel_sendData(SEQ_CC, SEQ_RUN_STOP,
					buttonHandler_stateMemory.seqRunning);
		}
		break;

	case BUT_REC:
		if (buttonHandler_getShift()) {
			menu_switchSubPage(0,0);
			menu_switchPage(RECORDING_PAGE);
		} else {
			//toggle the led
			buttonHandler_stateMemory.seqRecording = (uint8_t) ((1
					- buttonHandler_stateMemory.seqRecording) & 0x01);
			led_setValue(buttonHandler_stateMemory.seqRecording, LED_REC);
			//send run/stop command sequencer
			frontPanel_sendData(SEQ_CC, SEQ_REC_ON_OFF,
					buttonHandler_stateMemory.seqRecording);
		}
		break;

	case BUT_COPY:
		if (buttonHandler_getShift()) {

			//SONG MODE: while in song mode SHIFT+COPY = CLEAR and removes the current line from the pattern table
			if(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_SONG)
			{
				seq_deleteRow();
				menu_repaintAll();
			}
			//with shift -> clear mode. unless we are record active and playing. In this case,
			// holding down the copy button does a realtime erase on the active voice
			else if(buttonHandler_stateMemory.seqRecording && buttonHandler_stateMemory.seqRunning) {
				buttonHandler_stateMemory.seqErasing=1;
				frontPanel_sendData(SEQ_CC, SEQ_ERASE_ON_OFF,
						buttonHandler_stateMemory.seqErasing);
				// **RECORD todo update the display
			} else {

				if (copyClear_Mode == MODE_CLEAR) {
					//execute
					copyClear_executeClear();
					/*
					 copyClear_clearCurrentTrack();
					 copyClear_armClearMenu(0);
					 copyClear_Mode = MODE_NONE;
					 */
				} else {
					copyClear_Mode = MODE_CLEAR;
					copyClear_armClearMenu(1);
				}
			}

		} else {

			//SONG MODE: while in song mode SHIFT+COPY = CLEAR and removes the current line from the pattern table
			if(buttonHandler_stateMemory.selectButtonMode == SELECT_MODE_SONG)
			{
				seq_insertRow();
				menu_repaintAll();
			}
			else
			{
				//copy mode
				copyClear_Mode = MODE_COPY_TRACK;
				led_setBlinkLed(LED_COPY, 1);
				led_clearSelectLeds();
				led_clearVoiceLeds();
			}

		}
		break;

	case BUT_SEQ4:
		buttonHandler_seqButtonPressed(3);
		break;

	case BUT_SEQ3:
		buttonHandler_seqButtonPressed(2);
		break;

	case BUT_SEQ2:
		buttonHandler_seqButtonPressed(1);
		break;

	case BUT_SEQ1:
		buttonHandler_seqButtonPressed(0);
		break;

	default:
		break;
	}
}

//--------------------------------------------------------
void buttonHandler_buttonReleased(uint8_t buttonNr) {



	/*
	if(buttonNr < BUT_SELECT1) { // BUT_SEQ* buttons
		buttonHandler_seqButtonReleased(buttonNr);
		return;
	} else if(buttonNr < BUT_VOICE_1) { // BUT_SELECT* buttons
		buttonHandler_partButtonReleased((uint8_t)(buttonNr - BUT_SELECT1));
		return;
	}
*/
	// for the rest...
	switch (buttonNr) {


	case BUT_PAGE_UP:

			break;

	case BUT_PAGE_DOWN:

			break;

	case BUT_SELECT1:
		buttonHandler_partButtonReleased(0);
		break;
	case BUT_SELECT2:
		buttonHandler_partButtonReleased(1);
		break;
	case BUT_SELECT3:
		buttonHandler_partButtonReleased(2);
		break;
	case BUT_SELECT4:
		buttonHandler_partButtonReleased(3);
		break;
	case BUT_SELECT5:
		buttonHandler_partButtonReleased(4);
		break;
	case BUT_SELECT6:
		buttonHandler_partButtonReleased(5);
		break;
	case BUT_SELECT7:
		buttonHandler_partButtonReleased(6);
		break;
	case BUT_SELECT8:
		buttonHandler_partButtonReleased(7);
		break;

	case BUT_SEQ1:
		buttonHandler_seqButtonReleased(0);
		break;
	case BUT_SEQ2:
		buttonHandler_seqButtonReleased(1);
		break;
	case BUT_SEQ3:
		buttonHandler_seqButtonReleased(2);
		break;
	case BUT_SEQ4:
		buttonHandler_seqButtonReleased(3);
		break;
	case BUT_SEQ5:
		buttonHandler_seqButtonReleased(4);
		break;
	case BUT_SEQ6:
		buttonHandler_seqButtonReleased(5);
		break;
	case BUT_SEQ7:
		buttonHandler_seqButtonReleased(6);
		break;
	case BUT_SEQ8:
		buttonHandler_seqButtonReleased(7);
		break;
	case BUT_SEQ9:
		buttonHandler_seqButtonReleased(8);
		break;
	case BUT_SEQ10:
		buttonHandler_seqButtonReleased(9);
		break;
	case BUT_SEQ11:
		buttonHandler_seqButtonReleased(10);
		break;
	case BUT_SEQ12:
		buttonHandler_seqButtonReleased(11);
		break;
	case BUT_SEQ13:
		buttonHandler_seqButtonReleased(12);
		break;
	case BUT_SEQ14:
		buttonHandler_seqButtonReleased(13);
		break;
	case BUT_SEQ15:
		buttonHandler_seqButtonReleased(14);
		break;
	case BUT_SEQ16:
		buttonHandler_seqButtonReleased(15);
		break;

	case BUT_MODE3:
		if(buttonHandler_stateMemory.selectButtonMode != SELECT_MODE_LOAD)
		{
			led_setValue(0,LED_MODE3);
		}
		break;

	case BUT_MODE4:
		if(buttonHandler_stateMemory.selectButtonMode != SELECT_MODE_SAVE)
		{
			led_setValue(0,LED_MODE4);
		}
		break;


	case BUT_COPY:

		if(buttonHandler_stateMemory.seqErasing) {
			// **RECORD if we are in erase mode, exit that mode
			buttonHandler_stateMemory.seqErasing=0;
			frontPanel_sendData(SEQ_CC, SEQ_ERASE_ON_OFF,
									buttonHandler_stateMemory.seqErasing);
		} else {
			if (!buttonHandler_getShift()) {
				//copy mode abort/exit
				copyClear_reset();
			}
		}


		break;

	case BUT_SHIFT: // when this button is released, revert back to normal operating mode

		if(buttonHandler_stateMemory.seqErasing) {
			// **RECORD if we are in erase mode, exit that mode
			buttonHandler_stateMemory.seqErasing=0;
			frontPanel_sendData(SEQ_CC, SEQ_ERASE_ON_OFF,
						buttonHandler_stateMemory.seqErasing);
		}

		if (copyClear_Mode == MODE_CLEAR) {
			copyClear_armClearMenu(0);
			copyClear_Mode = MODE_NONE;
		}
		led_setValue(0, LED_SHIFT);

		switch(buttonHandler_stateMemory.selectButtonMode) {

		case SELECT_MODE_LAST_STEP_ALL:
		case SELECT_MODE_LAST_STEP_TRACK:
			//return to normal mode
		//	buttonHandler_handleModeButtons(SELECT_MODE_VOICE);
			break;

		case SELECT_MODE_VOICE:
		//	buttonHandler_leaveSeqMode();
			break;
		case SELECT_MODE_PERF:
			/*
			led_clearAllBlinkLeds();
			led_clearSelectLeds();
			menu_switchPage(PERFORMANCE_PAGE);
			led_initPerformanceLeds();
			*/
			return;
		case SELECT_MODE_PAT_GEN:
			/*
			//led_clearAllBlinkLeds();
			led_clearSelectLeds();
			led_setValue(1, led_idx2SelectLed(menu_getViewedPattern()));
			menu_switchPage(EUKLID_PAGE);
			*/
			break;
		case SELECT_MODE_STEP:
			//buttonHandler_enterSeqModeStepMode();
			break;
		default:
			break;
		}

		//show active voice if released
		if (buttonHandler_stateMemory.selectButtonMode != SELECT_MODE_PERF) {
			led_setActiveVoice(menu_getActiveVoice());
		} else {
			// TODO this code is never reached (see return above) ???
			buttonHandler_showMuteLEDs();
		}

		break;
	default:
		break;
	}
}

//--------------------------------------------------------------
//void buttonHandler_toggleEuklidMode() {
//	if (menu_getActivePage() == PERFORMANCE_PAGE) {
//		//switch to Euklid
//		menu_switchPage(EUKLID_PAGE);
//	} else {
//		//switch back to mute mode
//		menu_switchPage(PERFORMANCE_PAGE);
//	}
//
//}
//--------------------------------------------------------------
uint8_t buttonHandler_getMode() {
	return buttonHandler_stateMemory.selectButtonMode;
};
//--------------------------------------------------------------
void buttonHandler_setMode(uint8_t mode)
{
	buttonHandler_stateMemory.selectButtonMode = mode;
}
//--------------------------------------------------------------

void buttonHandler_setRunStopState(uint8_t running) {
	// set the state
	buttonHandler_stateMemory.seqRunning = (uint8_t) (running & 0x01);
	// set the led
	led_setValue(buttonHandler_stateMemory.seqRunning, LED_START_STOP);
}
//--------------------------------------------------------------
void buttonHandler_init()
{
	//update the status LED
	led_setMode2(buttonHandler_stateMemory.selectButtonMode);
	//set menu to voice page mode
	menu_switchPage(menu_getActiveVoice());
	led_setActiveSelectButton(menu_getSubPage());
	menu_resetActiveParameter();
}
