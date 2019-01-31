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

#ifndef __FPSGO_USEDEXT_H__
#define __FPSGO_USEDEXT_H__

extern void (*cpufreq_notifier_fp)(int, unsigned long);
extern void (*fpsgo_notify_qudeq_fp)(int qudeq, unsigned int startend,
		unsigned long long bufID, int pid, int queue_SF);
extern void (*fpsgo_notify_intended_vsync_fp)(int pid,
		unsigned long long frame_id);
extern void (*fpsgo_notify_framecomplete_fp)(int ui_pid,
		unsigned long long frame_time,
		int render_method, int render, unsigned long long frame_id);
extern void (*fpsgo_notify_connect_fp)(int pid,
		unsigned long long bufID, int connectedAPI);
extern void (*fpsgo_notify_draw_start_fp)(int pid, unsigned long long frame_id);

extern void (*ged_vsync_notifier_fp)(void);

extern int (*fpsgo_fbt2fstb_cpu_capability_fp)(
	int pid, int frame_type,
	unsigned int curr_cap,
	unsigned int max_cap,
	unsigned int target_fps,
	unsigned long long Q2Q_time,
	unsigned long long Running_time);

int fpsgo_is_force_enable(void);
void fpsgo_force_switch_enable(int enable);

#endif
