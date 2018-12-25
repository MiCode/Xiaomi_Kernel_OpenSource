/*
 * Copyright (c) 2014 TRUSTONIC LIMITED
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

#ifndef __TUI_HAL_MT__H__
#define __TUI_HAL_MT__H__

/* for TUI mepping to Security World */
extern int tpd_enter_tui(void);
extern int tpd_exit_tui(void);
extern int i2c_tui_enable_clock(int id);
extern int i2c_tui_disable_clock(int id);
extern int tui_region_offline(phys_addr_t *pa, unsigned long *size);
extern int tui_region_online(void);
extern int display_enter_tui(void);
extern int display_exit_tui(void);
#endif
