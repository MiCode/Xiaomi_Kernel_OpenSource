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

#ifndef FSTB_H
#define FSTB_H

int mtk_fstb_exit(void);
int mtk_fstb_init(void);
static inline int fpsgo_ctrl2fstb_display_time_update(unsigned long long ns)
{ return 0; }
static inline int fpsgo_ctrl2fstb_gpu_time_update(
		long long t_gpu, unsigned int cur_freq,
		unsigned int cur_max_freq) { return 0; }
void fpsgo_comp2fstb_queue_time_update(
	int pid, int frame_type,
	unsigned long long ts,
	unsigned long long bufferid, int api);
int fpsgo_comp2fstb_enq_end(int pid, unsigned long long enq);
int fpsgo_ctrl2fstb_gblock(int tid, int start);
void fpsgo_comp2fstb_camera_active(int pid);

#if defined(CONFIG_MTK_FPSGO) || defined(CONFIG_MTK_FPSGO_V3)
int fpsgo_fstb2fbt_reset_asfc(int value);
int is_fstb_enable(void);
int is_fstb_active(long long time_diff);
int fpsgo_ctrl2fstb_switch_fstb(int value);
int switch_sample_window(long long time_usec);
int switch_fps_range(int nr_level, struct fps_level *level);
int switch_process_fps_range(char *proc_name,
	int nr_level, struct fps_level *level);
int switch_thread_fps_range(pid_t pid, int nr_level, struct fps_level *level);
int switch_dfps_ceiling(int fps);
int switch_fps_error_threhosld(int threshold);
int switch_percentile_frametime(int ratio);
int fpsgo_fbt2fstb_update_cpu_frame_info(
	int pid,
	int tgid,
	int frame_type,
	unsigned long long Q2Q_time,
	unsigned long long Runnging_time,
	unsigned int Curr_cap,
	unsigned int Max_cap,
	unsigned long long mid);
void fpsgo_fbt2fstb_query_fps(int pid, int *target_fps,
			int *target_cpu_time, int tgid, unsigned long long mid);
int fpsgo_fbt2fstb_query_fteh_list(int pid);
void fpsgo_ctrl2fstb_dfrc_fps(int dfrc_fps);

#else
static inline int fpsgo_fstb2fbt_reset_asfc(int level) { return 0; }
static inline int is_fstb_enable(void) { return 0; }
static inline int fpsgo_ctrl2fstb_switch_fstb(int en) { return 0; }
static inline int switch_sample_window(long long time_usec) { return 0; }
static inline int switch_fps_range(int nr_level,
	struct fps_level *level) { return 0; }
static inline int switch_process_fps_range(char *proc_name,
	int nr_level, struct fps_level *level) { return 0; }
static inline int switch_thread_fps_range(pid_t pid,
	int nr_level, struct fps_level *level) { return 0; }
static inline int switch_dfps_ceiling(int fps) { return 0; }
static inline int switch_fps_error_threhosld(int threshold) { return 0; }
static inline int switch_percentile_frametime(int ratio) { return 0; }
static inline int fpsgo_fbt2fstb_update_cpu_frame_info(
	int pid,
	int tgid,
	int frame_type,
	unsigned long long Q2Q_time,
	unsigned long long Runnging_time,
	unsigned int Curr_cap,
	unsigned int Max_cap,
	unsigned long long mid) { return 0; }
static inline void fpsgo_fbt2fstb_query_fps(int pid,
			int *target_fps, int *target_cpu_time,
			int tgid, unsigned long long mid) { }
static inline int fpsgo_fbt2fstb_query_fteh_list(int pid) { return 0; }
static void fpsgo_ctrl2fstb_dfrc_fps(int dfrc_fps) { }

#endif

#endif

