/*
 * projectBrowse.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * Kits of other projects in the kit lists of the load voice, load kit and
 * save kit menus: "Change Project" (below kit 0) shows the project list
 * ("Select Project:"), confirming a project lists its kits. The active
 * project's kits come from RAM (incl. unsaved changes), other projects' from
 * their folder on the SD card.
 */

#ifndef PROJECTBROWSE_H_
#define PROJECTBROWSE_H_

#include "stm32f7xx_hal.h"
#include "ff.h"

typedef struct
{
	uint8_t project;		//the project whose kits are listed
	uint8_t selecting;		//the project list is shown
	uint8_t projectSel;		//position in the project list
	uint8_t changeRow;		//"Change Project" (below kit 0) is selected
} ProjectBrowse;

extern ProjectBrowse projectBrowse;

//list the active project's kits
void projectBrowse_reset(void);
//are the kits of another project than the active one listed?
uint8_t projectBrowse_isOther(void);

//encoder on the kit list: handles "Change Project" and the project list,
//returns 1 if it did (otherwise the kit number changes as usual)
uint8_t projectBrowse_encoder(const int8_t inc, uint8_t* kitNr);
//encoder button on the kit list, returns 1 if it was for the project list or
//"Change Project" (forSave: only existing projects can be chosen)
uint8_t projectBrowse_click(const uint8_t forSave);

//the listed project's kits
const char* projectBrowse_kitName(const uint8_t kit);		//8 characters, "Empty" if none
uint8_t projectBrowse_kitExists(const uint8_t kit);
uint8_t projectBrowse_kitEngine(const uint8_t kit, const uint8_t slot);
FRESULT projectBrowse_readKit(const uint8_t kit, uint8_t* params, uint8_t* engines);
//save the active kit into the listed (other) project
FRESULT projectBrowse_saveKit(const uint8_t kit, char* name);

//the 2 display lines for the project list or "Change Project"; returns 1 if
//it drew line 1 (and line 0 for the project list)
uint8_t projectBrowse_repaint(char lines[2][17]);
//" pNN" at the end of the top line while another project is listed
void projectBrowse_repaintProjectNr(char* line);

#endif /* PROJECTBROWSE_H_ */
