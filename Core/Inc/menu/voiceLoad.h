/*
 * voiceLoad.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * Load a voice from another kit, also from other projects: LOAD + step button
 * 1-7 (the voice buttons of the KIT menu). Works like the load kit menu: the
 * encoder chooses the kit, the encoder button loads the voice of that kit
 * (same voice, with its engine) into the active kit. "Change Project" (below
 * kit 0) lists the projects; confirming one lists its kits.
 */

#ifndef VOICELOAD_H_
#define VOICELOAD_H_

#include "stm32f7xx_hal.h"

//LOAD + step button 1-7, or a voice button while the menu is open
void voiceLoad_open(const uint8_t voice);
void voiceLoad_encoder(const int8_t inc);
void voiceLoad_click(void);
//another page is opened
void voiceLoad_leave(void);
void voiceLoad_repaint(char lines[2][17]);

#endif /* VOICELOAD_H_ */
