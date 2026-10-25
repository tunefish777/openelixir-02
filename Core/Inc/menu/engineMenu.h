/*
 * engineMenu.h
 *
 * Copyright (C) 2026 tunefish.at
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of OpenElixir-02, based on the LXR firmware. It is
 * licensed under the GNU General Public License version 3, with the
 * additional permission and terms in NOTICE.md. See LICENSE and NOTICE.md in
 * the root of the repository.
 *
 * Engine menu, a page like the load/save menu: select a voice and press
 * SHIFT + KIT to open it (it stays open when SHIFT is released; the step LEDs
 * don't show the voice's steps). The encoder chooses the engine, the encoder
 * button applies it, the voice buttons select the voice. Any mode button
 * switches to another menu.
 */

#ifndef ENGINEMENU_H_
#define ENGINEMENU_H_

#include "stm32f7xx_hal.h"
#include "menu/menu.h"

#define engineMenu_shown	(menu_activePage == ENGINE_PAGE)

//SHIFT + KIT or voice button in the menu: open it for the active voice
void engineMenu_open(void);
//encoder click: apply
void engineMenu_apply(void);
//encoder turned
void engineMenu_encoder(const int8_t inc);
//draw the menu into the 2 LCD lines
void engineMenu_repaint(char lines[2][17]);

#endif /* ENGINEMENU_H_ */
