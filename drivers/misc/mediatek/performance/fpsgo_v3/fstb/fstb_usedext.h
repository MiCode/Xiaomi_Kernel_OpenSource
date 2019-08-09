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

#ifndef FSTB_USEDEXT_H
#define FSTB_USEDEXT_H

#include <fpsgo_common.h>
#include <trace/events/fpsgo.h>
#include <linux/list.h>
#include <linux/sched.h>

#define CFG_MAX_FPS_LIMIT	60
#define CFG_MIN_FPS_LIMIT	10
#define FRAME_TIME_BUFFER_SIZE 200
#define MAX_NR_FPS_LEVELS	1
#define MAX_NR_RENDER_FPS_LEVELS	5
#define DISPLAY_FPS_FILTER_NS 100000000ULL
#define ASFC_THRESHOLD_NS 20000000ULL
#define ASFC_THRESHOLD_PERCENTAGE 30
#define VPU_MAX_CAP 100
#define MDLA_MAX_CAP 100
#define RESET_TOLERENCE 3

static int max_fps_limit = CFG_MAX_FPS_LIMIT;
static int dfps_ceiling = CFG_MAX_FPS_LIMIT;
static int min_fps_limit = CFG_MIN_FPS_LIMIT;
static int fps_error_threshold = 10;
static int QUANTILE = 50;
static long long FRAME_TIME_WINDOW_SIZE_US = 1000000;
static long long ADJUST_INTERVAL_US = 1000000;
static int margin_mode;

extern int (*fbt_notifier_cpu_frame_time_fps_stabilizer)(
	int pid,
	int frame_type,
	unsigned long long Q2Q_time,
	unsigned long long Runnging_time,
	unsigned int Curr_cap,
	unsigned int Max_cap,
	unsigned int Target_fps);
extern void (*display_time_fps_stablizer)(unsigned long long ts);
extern void (*ged_kpi_output_gfx_info2_fp)(long long t_gpu,
	unsigned int cur_freq, unsigned int cur_max_freq, u64 ulID);

struct FSTB_FRAME_INFO {
	struct hlist_node hlist;

	int pid;
	int target_fps;
	int target_fps_margin;
	int queue_fps;
	unsigned long long bufid;
	int asfc_flag;
	int in_list;
	int check_asfc;
	int new_info;

	long long m_c_time;
	unsigned int m_c_cap;
	long long m_v_time;
	unsigned int m_v_cap;
	long long m_m_time;
	unsigned int m_m_cap;

	long long gpu_time;
	int gpu_freq;

	unsigned long long queue_time_ts[FRAME_TIME_BUFFER_SIZE]; /*timestamp*/
	int queue_time_begin;
	int queue_time_end;
	unsigned long long weighted_cpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_cpu_time_ts[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_gpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long weighted_gpu_time_ts[FRAME_TIME_BUFFER_SIZE];
	int weighted_cpu_time_begin;
	int weighted_cpu_time_end;
	int weighted_gpu_time_begin;
	int weighted_gpu_time_end;
	unsigned long long sorted_weighted_cpu_time[FRAME_TIME_BUFFER_SIZE];
	unsigned long long sorted_weighted_gpu_time[FRAME_TIME_BUFFER_SIZE];

	unsigned long long gblock_b;
	unsigned long long gblock_time;
};

struct FSTB_RENDER_TARGET_FPS {
	struct hlist_node hlist;

	char process_name[16];
	int pid;
	int nr_level;
	struct fps_level level[MAX_NR_RENDER_FPS_LEVELS];
};

struct FSTB_FTEH_LIST {
	struct hlist_node hlist;
	char process_name[16];
	char thread_name[16];
};

#endif

