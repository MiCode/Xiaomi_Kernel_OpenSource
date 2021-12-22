/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef SYSLIMITER_H
#define SYSLIMITER_H

#include <linux/proc_fs.h>

extern void syslimiter_update_dfrc_fps(int fps);
extern void syslimiter_update_fpsgo_state(int state);

extern int syslimiter_init(struct proc_dir_entry *parent);
extern void syslimiter_exit(void);
#endif

