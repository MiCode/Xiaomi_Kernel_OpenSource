/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

extern int cpu_eff_tp;
extern unsigned long long big_cpu_eff_tp;
/* Stune group info */
#ifdef CONFIG_SCHED_TUNE
extern int group_boost_read(int group_idx);
extern int group_prefer_idle_read(int group_idx);
#else
static int group_boost_read(int group_idx) { return 0; }
static int group_prefer_idle_read(int group_idx) { return 0; }
#endif

