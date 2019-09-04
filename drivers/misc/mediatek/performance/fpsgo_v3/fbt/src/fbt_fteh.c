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
#include <linux/module.h>

#include <fpsgo_common.h>

#include <trace/events/fpsgo.h>

#include "fbt_fteh.h"
#include "xgf.h"

#define MAX_CAP 100
#define MAX_RECORD_COUNT 16
#define TIME_MS_TO_NS  1000000ULL
#define TIME_1S  1000000000

#define FPSGO_FTEH_DEBUG

#ifdef FPSGO_FTEH_DEBUG
#define FPSGO_FTEH_TRACE(...)	xgf_trace("fpsgo_fteh:" __VA_ARGS__)
#else
#define FPSGO_FTEH_TRACE(...)
#endif

enum FTEH_STATE {
	FTEH_INACTIVE = 0,
	FTEH_PRESTARTLM = 1,
	FTEH_STARTLM = 2,
	FTEH_ACTIVE = 3,
};

enum FTEH_RESULT {
	FTEH_LESS_HEADROOM = 0,
	FTEH_CORRECT = 1,
};

enum FTEH_LOADING_SUM {
	FTEH_HEAVY_LOADING = 0,
	FTEH_LIGHT_LOADING = 1,
	FTEH_INVALID_LOADING = 2,
};

static int g_fteh_state;
static int g_cur_tracking_pid;
static unsigned long long g_load_ts;
static struct fpsgo_loading *g_load_arr;
static int g_load_arr_size;
static int g_load_valid_size;
static int g_prestart_count;
static int g_debounce_count;

static int cap_th;
static int time_th;
static int loading_th;
static int sampling_period_MS;
static int start_th;
static int debounce_th;
static int enter_loading_th;

module_param(cap_th, int, 0644);
module_param(time_th, int, 0644);
module_param(loading_th, int, 0644);
module_param(sampling_period_MS, int, 0644);
module_param(start_th, int, 0644);
module_param(debounce_th, int, 0644);
module_param(enter_loading_th, int, 0644);


int fbt_fteh_set_cap_threshold(int threshold)
{
	if (threshold < 0 || threshold > 100)
		return -EINVAL;

	cap_th = threshold;
	return 0;
}

int fbt_fteh_set_sleeptime_threshold(int threshold)
{
	if (threshold < 0 || threshold > 100)
		return -EINVAL;

	time_th = threshold;
	return 0;
}

int fbt_fteh_set_enter_loading_threshold(int threshold)
{
	if (threshold < 0 || threshold > 100)
		return -EINVAL;

	enter_loading_th = threshold;
	return 0;
}

int fbt_fteh_set_loading_threshold(int threshold)
{
	if (threshold < 0 || threshold > 100)
		return -EINVAL;

	loading_th = threshold;
	return 0;
}

int fbt_fteh_set_sampling_period_MS(int period)
{
	if (period < 32)
		return -EINVAL;

	sampling_period_MS = period;
	return 0;
}

int fbt_fteh_set_start_threshold(int threshold)
{
	if (threshold < 0)
		return -EINVAL;

	start_th = threshold;
	return 0;
}

int fbt_fteh_set_debounce_threshold(int threshold)
{
	if (threshold < 0)
		return -EINVAL;

	debounce_th = threshold;
	return 0;
}

static int fteh_is_little_sleep_time(unsigned long long q2q_time,
			unsigned long long sleep_time)
{
	unsigned long long temp_time;

	if (!sleep_time)
		return 1;

	temp_time = q2q_time * time_th;
	do_div(temp_time, 100U);

	if (sleep_time < temp_time)
		return 1;
	else
		return 0;
}

static int fteh_is_max_capacity(unsigned int blc_wt)
{
	if (blc_wt >= cap_th)
		return 1;
	else
		return 0;
}

static int fteh_is_light_loading(int th, int check)
{
	int i, sum = 0;

	if (!g_load_arr || !g_load_valid_size)
		return FTEH_HEAVY_LOADING;

	for (i = 0; i < g_load_valid_size; i++) {
		FPSGO_FTEH_TRACE("[%d] %d", g_load_arr[i].pid,
					g_load_arr[i].loading);
		sum += (g_load_arr[i].loading);
	}

	if (check && !sum)
		return FTEH_INVALID_LOADING;

	if (sum > th)
		return FTEH_HEAVY_LOADING;

	return FTEH_LIGHT_LOADING;
}

static int fteh_check_to_start_monitoring(void)
{
	if (g_prestart_count >= start_th)
		return 1;
	else
		return 0;
}

static int fteh_check_to_debounce(void)
{
	FPSGO_FTEH_TRACE("debounce_count %d", g_debounce_count);

	if (g_debounce_count >= debounce_th)
		return 1;
	else
		return 0;
}

static void fteh_reset_state(void)
{
	if (g_fteh_state == FTEH_STARTLM || g_fteh_state == FTEH_ACTIVE)
		fpsgo_fteh2minitop_end();

	g_fteh_state = FTEH_INACTIVE;
	g_load_valid_size = 0;
	g_prestart_count = 0;
	g_debounce_count = 0;
}

static int fteh_update_dep_list_start(int pid)
{
	int count = 0;
	int ret_size;

	if (!pid)
		return 0;

	count = fpsgo_fteh2xgf_get_dep_list_num(pid);
	if (count <= 0) {
		g_load_valid_size = 0;
		fpsgo_systrace_c_fbt(pid, count, "dep_list_num");
		return 0;
	}

	count = clamp(count, 1, MAX_RECORD_COUNT);

	if (g_load_arr_size == 0) {
		g_load_arr = (struct fpsgo_loading *)
			fpsgo_alloc_atomic(count *
					sizeof(struct fpsgo_loading));
		g_load_arr_size = count;
	} else if (g_load_arr_size < count) {
		fpsgo_free(g_load_arr,
			g_load_arr_size * sizeof(struct fpsgo_loading));
		g_load_arr = (struct fpsgo_loading *)
			fpsgo_alloc_atomic(count *
					sizeof(struct fpsgo_loading));
		g_load_arr_size = count;
	}

	if (g_load_arr == NULL) {
		g_load_arr_size = 0;
		g_load_valid_size = 0;
		return 0;
	}

	g_load_valid_size = count;
	memset(g_load_arr, 0, g_load_arr_size * sizeof(struct fpsgo_loading));
	ret_size = fpsgo_fteh2xgf_get_dep_list(pid,
			g_load_valid_size, g_load_arr);
	if (ret_size > g_load_arr_size || ret_size == 0) {
		fpsgo_systrace_c_fbt(pid, ret_size, "ret_size");
		return 0;
	}
	fpsgo_fteh2minitop_start(ret_size, g_load_arr);

	return 1;
}

static int fteh_loading_monitor(int pid)
{
	int ret = 0;

	if (g_load_valid_size <= 0 || !g_load_arr)
		goto RESET;

	fpsgo_fteh2minitop_query(g_load_valid_size, g_load_arr);

	ret = fteh_is_light_loading(enter_loading_th, 1);
	if (ret == FTEH_INVALID_LOADING) {
		fteh_update_dep_list_start(pid);
		return FTEH_CORRECT;
	}

	if (ret == FTEH_LIGHT_LOADING) {
		g_fteh_state = FTEH_ACTIVE;
		fteh_update_dep_list_start(pid);
		return FTEH_LESS_HEADROOM;
	}

RESET:
	fteh_reset_state();
	return FTEH_CORRECT;
}

static int fteh_keep_active(int pid, unsigned long long q2q_time,
		unsigned long long sleep_time)
{
	if (g_load_valid_size <= 0 || !g_load_arr)
		goto debounce;

	fpsgo_fteh2minitop_query(g_load_valid_size, g_load_arr);

	if (fteh_is_light_loading(loading_th, 0) &&
		fteh_is_little_sleep_time(q2q_time, sleep_time)) {
		g_debounce_count = 0;
		goto keep;
	} else {
		g_debounce_count++;
		if (fteh_check_to_debounce())
			goto debounce;
		else
			goto keep;
	}

debounce:
	fteh_reset_state();
	return FTEH_CORRECT;
keep:
	fteh_update_dep_list_start(pid);
	return FTEH_LESS_HEADROOM;
}

static void fteh_render_task_changed(int pid)
{
	if (g_fteh_state != FTEH_INACTIVE)
		fteh_reset_state();

	g_load_ts = 0ULL;
	g_cur_tracking_pid = pid;
}

int fpsgo_fbt2fteh_judge_ceiling(struct render_info *thread_info,
			unsigned int blc_wt)
{
	unsigned long long cur_time;

	if (!thread_info)
		return FTEH_CORRECT;

	cur_time = fpsgo_get_time();

	if (thread_info->pid != g_cur_tracking_pid ||
		(g_load_ts && cur_time > g_load_ts &&
			cur_time - g_load_ts > TIME_1S))
		fteh_render_task_changed(thread_info->pid);

	if (cur_time <= g_load_ts ||
		(g_load_ts && cur_time - g_load_ts <
		(unsigned long long)sampling_period_MS * TIME_MS_TO_NS)) {
		if (g_fteh_state == FTEH_ACTIVE)
			return FTEH_LESS_HEADROOM;
		else
			return FTEH_CORRECT;
	}
	g_load_ts = cur_time;

	fpsgo_systrace_c_fbt_gm(thread_info->pid, g_fteh_state, "fteh_state");

	if (g_fteh_state == FTEH_ACTIVE)
		return fteh_keep_active(thread_info->pid, thread_info->Q2Q_time,
			thread_info->Q2Q_time - thread_info->running_time);

	if (!(fteh_is_max_capacity(blc_wt) &&
		fteh_is_little_sleep_time(thread_info->Q2Q_time,
			thread_info->Q2Q_time - thread_info->running_time))) {
		fteh_reset_state();
		return FTEH_CORRECT;
	}

	if (g_fteh_state == FTEH_STARTLM)
		return fteh_loading_monitor(thread_info->pid);

	if (g_fteh_state != FTEH_PRESTARTLM) {
		g_prestart_count = 0;
		g_fteh_state = FTEH_PRESTARTLM;
		return FTEH_CORRECT;
	}

	g_prestart_count++;
	if (fteh_check_to_start_monitoring()) {
		if (fteh_update_dep_list_start(thread_info->pid)) {
			g_prestart_count = 0;
			g_fteh_state = FTEH_STARTLM;
		}
	}

	return FTEH_CORRECT;
}

int fpsgo_fteh_get_state(int *pid)
{
	*pid = g_cur_tracking_pid;

	return g_fteh_state;
}

void __exit fbt_fteh_exit(void)
{
	fpsgo_free(g_load_arr, g_load_arr_size * sizeof(struct fpsgo_loading));
	g_load_arr_size = 0;
	g_load_valid_size = 0;
}

int __init fbt_fteh_init(void)
{
	cap_th = 85;
	time_th = 15;
	enter_loading_th = 60;
	loading_th = 80;
	sampling_period_MS = 256;
	start_th = 3;
	debounce_th = 3;

	return 0;
}
