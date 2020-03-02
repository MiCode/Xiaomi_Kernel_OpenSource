/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _TUI_HAL_H_
#define _TUI_HAL_H_

#include <linux/types.h>

uint32_t hal_tui_init(void);
void hal_tui_exit(void);
uint32_t hal_tui_deactivate(void);
uint32_t hal_tui_activate(void);

#endif
