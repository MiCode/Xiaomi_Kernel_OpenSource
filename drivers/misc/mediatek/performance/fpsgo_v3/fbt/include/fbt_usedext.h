/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __FBT_USEDEXT_H__
#define __FBT_USEDEXT_H__

extern int prefer_idle_for_perf_idx(int idx, int prefer_idle);
extern unsigned int mt_cpufreq_get_freq_by_idx(int id, int idx);
extern unsigned int mt_ppm_userlimit_freq_limit_by_others(
		unsigned int cluster);
extern unsigned long get_cpu_orig_capacity(unsigned int cpu);
extern int upower_get_turn_point(void);
extern void set_capacity_margin(unsigned int margin);
extern unsigned int get_capacity_margin(void);
extern void set_user_nice(struct task_struct *p, long nice);

extern int fpsgo_fbt2minitop_start(int count, struct fpsgo_loading *fl);
extern int fpsgo_fbt2minitop_query(int count, struct fpsgo_loading *fl);
extern int fpsgo_fbt2minitop_end(void);
extern int fpsgo_fbt2minitop_query_single(pid_t pid);

extern struct workqueue_struct *g_psNotifyWorkQueue;

#endif
