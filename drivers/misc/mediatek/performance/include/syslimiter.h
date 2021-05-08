/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef SYSLIMITER_H
#define SYSLIMITER_H

#include <linux/proc_fs.h>

extern void syslimiter_update_dfrc_fps(int fps);
extern void syslimiter_update_fpsgo_state(int state);

extern int syslimiter_init(struct proc_dir_entry *parent);
extern void syslimiter_exit(void);
#endif

