/*
 * VelocityModulation.h
 *
 *  Created on: 06.01.2013
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#ifndef VELOCITYMODULATION_H_
#define VELOCITYMODULATION_H_

#include "stm32f7xx_hal.h"
#include "Soundengine/MIDI/ParameterArray.h"

//LFO destinations of the target group "Perf" that are no sound parameters (the DJ
//filter): numbers right after the sound parameters (the LFO target is sent as 8 bit)
#define MOD_DEST_DJ_FILTER	(END_OF_SOUND_PARAMETERS)
#define MOD_DEST_DJ_RESO	(END_OF_SOUND_PARAMETERS+1)

typedef struct ModulatorStruct
{

	uint16_t	destination;	/**< dest param nr */
	uint8_t		type;			/**< pointer type */
	ptrValue	originalValue;	/**< stores the original value of the parameter*/
	float		amount;			/**< modulation amount*/
	float 		lastVal;

} ModulationNode;

//TODO move into corresponding voice
extern ModulationNode velocityModulators[6];

void modNode_init(ModulationNode* vm);
void modNode_resetTargets();
void modNode_reassignVeloMod();

/** if multiple nodes address the same target we need to update the other modNodes if one of them changes the destionation*/
//void modNode_originalValueModulated(uint16_t idx, ModulationNode* modSource);
void modNode_originalValueChanged(uint16_t idx);
void modNode_setDestination(ModulationNode* vm, uint16_t dest);
void modNode_updateValue(ModulationNode* vm, float val);
#endif /* VELOCITYMODULATION_H_ */
