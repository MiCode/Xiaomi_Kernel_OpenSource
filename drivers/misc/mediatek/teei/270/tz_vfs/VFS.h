/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
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

#ifndef __TZ_VFS_H__
#define __TZ_VFS_H__

#ifdef CONFIG_MICROTRUST_TUI_DRIVER
extern int display_enter_tui(void);
extern int display_exit_tui(void);
extern int primary_display_trigger(int blocking,
				void *callback, int need_merge);
extern void mt_deint_leave(void);
extern void mt_deint_restore(void);
extern int tui_i2c_enable_clock(void);
extern int tui_i2c_disable_clock(void);
#endif

#endif /* __TZ_VFS_H__ */
