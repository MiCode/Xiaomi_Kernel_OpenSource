/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
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
