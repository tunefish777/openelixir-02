/* Generated no-op stubs for symbols outside the audio engine (sequencer, UI, UART, USB, codec). */
#include "stm32f7xx_hal.h"
#include "IO/adcPots.h"
#include "Preset/PresetManager.h"
#include "Soundengine/AudioCodecManager.h"
#include "Soundengine/DSPAudio/random.h"
#include "Soundengine/MIDI/Uart.h"
#include "Soundengine/Sequencer/EuklidGenerator.h"
#include "Soundengine/Sequencer/clockSync.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "Soundengine/TriggerOut.h"
#include "Soundengine/USB/usb_manager.h"
#include "Soundengine/cs4344_cs5343.h"
#include "Soundengine/globals.h"
#include "ledHandler.h"
#include "main.h"
#include "menu/menu.h"
int CodecInit(void) { return 0; }
void preset_touchTimer(PresetTimer type) { }
uint8_t euklid_getLength(uint8_t trackNr) { return 0; }
uint8_t euklid_getRotation(uint8_t trackNr) { return 0; }
uint8_t euklid_getSteps(uint8_t trackNr) { return 0; }
void euklid_init(void) { }
void euklid_setLength(uint8_t trackNr, uint8_t value) { }
void euklid_setRotation(uint8_t trackNr, uint8_t value, uint8_t patternNr) { }
void euklid_setSteps(uint8_t trackNr, uint8_t value, uint8_t patternNr) { }
void led_updateSeqLeds(void) { }
uint8_t menu_getActiveVoice(void) { return 0; }
uint8_t menu_getSubPage(void) { return 0; }
uint8_t menu_getViewedPattern(void) { return 0; }
void menu_repaint(void) { }
void menu_switchSubPage(uint8_t subPageNr, uint8_t toggle) { }
void preset_morph_tick(void) { }
void seq_addNote(uint8_t trackNr,uint8_t vel, uint8_t note) { }
void seq_armAutomationStep(uint8_t stepNr, uint8_t track,uint8_t isArmed) { }
void seq_clearAutomation(uint8_t trackNr, uint8_t pattern, uint8_t automTrack) { }
void seq_clearTrack(uint8_t trackNr, uint8_t pattern) { }
void seq_copyPattern(uint8_t src, uint8_t dst) { }
void seq_copyTrack(uint8_t srcNr, uint8_t dstNr, uint8_t pattern) { }
uint16_t seq_getBpm(void) { return 0; }
uint8_t seq_getExtSync(void) { return 0; }
void seq_init(void) { }
uint8_t seq_isAutomationStepArmed(void) { return 0; }
uint8_t seq_isRunning(void) { return 0; }
uint8_t seq_isTrackMuted(uint8_t trackNr) { return 0; }
void seq_midiNoteOff(uint8_t chan) { }
void seq_recordAutomation(uint8_t voice, uint8_t dest, uint8_t value) { }
void seq_resetPosition(void) { }
void seq_sendMidiNoteOn(const uint8_t channel, const uint8_t note, const uint8_t veloc) { }
void seq_sendStepInfoToFront(uint16_t stepNr) { }
void seq_setActiveAutomationTrack(uint8_t trackNr) { }
void seq_setBpm(uint16_t bpm) { }
void seq_setErasingMode(uint8_t active) { }
void seq_setExtSync(uint8_t isExt) { }
void seq_setMute(uint8_t trackNr, uint8_t isMuted) { }
void seq_setNextPattern(const uint8_t patNr) { }
void seq_setQuantisation(uint8_t value) { }
void seq_setRecordingMode(uint8_t active) { }
void seq_setRoll(uint8_t voice, uint8_t onOff) { }
void seq_setRollRate(uint8_t rate) { }
void seq_setRunning(uint8_t isRunning) { }
void seq_setShuffle(float shuffle) { }
void seq_setTrackLength(uint8_t trackNr, uint8_t length) { }
void seq_sync(void) { }
void seq_tick(void) { }
void sync_midiResetPosition(void) { }
void sync_midiStartStop(uint8_t isStart) { }
void uart_clearFrontFifo(void) { }
void uart_processMidi(void) { }
void uart_sendFrontpanelByte(uint8_t data) { }
void uart_sendFrontpanelSysExByte(uint8_t data) { }
void uart_sendMidi(MidiMsg msg) { }
void usb_sendMidi(MidiMsg msg) { }
void usb_tick(void) { }
uint8_t activeParameters[NUM_PARAMS];
volatile  uint8_t bCurrentSampleValid;
int16_t dma_buffer[OUTPUT_DMA_SIZE*4];
int16_t dma_buffer2[OUTPUT_DMA_SIZE*4];
RNG_HandleTypeDef hrng;
TIM_HandleTypeDef htim2;
uint8_t menu_activePage;
PatternSet seq_patternSet;
uint8_t seq_resetBarOnPatternChange;
uint8_t seq_selectedStep;
volatile uint32_t systick_ticks;
uint8_t trigger_dividerClockOut1;
uint8_t trigger_prescalerClockInput;
