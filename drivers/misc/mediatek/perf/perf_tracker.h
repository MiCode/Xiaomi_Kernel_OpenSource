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


#ifndef _PERF_TRACKER_H
#define _PERF_TRACKER_H

#ifdef CONFIG_MTK_CPU_FREQ
extern unsigned int mt_cpufreq_get_cur_freq(int id);
#else
static inline  int mt_cpufreq_get_cur_freq(int id) { return 0; }
#endif

#endif /* _PERF_TRACKER_H */
