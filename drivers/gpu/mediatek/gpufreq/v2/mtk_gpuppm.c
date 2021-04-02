// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    mtk_gpuppm.c
 * @brief   Performance power management for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/mutex.h>

#include <mtk_gpufreq_v2.h>
#include <mtk_gpufreq_debug.h>
#include <mtk_gpufreq_common.h>
#include <mtk_gpuppm.h>
#if defined(CONFIG_GPU_MT6893)
#include <mt6893/mtk_gpufreq_core.h>
#endif

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static DEFINE_MUTEX(gpuppm_lock_gpu);
static DEFINE_MUTEX(gpuppm_lock_gstack);
struct gpuppm_status g_gpu;
struct gpuppm_status g_gstack;

struct gpuppm_limit_info g_gpu_limit_table[] = {
	LIMITOP(LIMIT_SEGMENT, "SEGMENT", GPUPPM_PRIO_9,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_DEBUG, "DEBUG", GPUPPM_PRIO_8,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_THERMAL, "THERMAL", GPUPPM_PRIO_7,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_SRAMRC, "SRAMRC", GPUPPM_PRIO_6,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_BATT_OC, "BATT_OC", GPUPPM_PRIO_5,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_BATT_PERCENT, "BATT_PERCENT", GPUPPM_PRIO_5,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_LOW_BATT, "LOW_BATT", GPUPPM_PRIO_5,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_PBM, "PBM", GPUPPM_PRIO_5,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_APIBOOST, "APIBOOST", GPUPPM_PRIO_4,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_FPSGO, "FPSGO", GPUPPM_PRIO_4,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
};

struct gpuppm_limit_info g_gstack_limit_table[] = {};

static void __gpuppm_sort_limit(enum gpufreq_target target);

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
static void __gpuppm_sort_limit(enum gpufreq_target target)
{
	struct gpuppm_limit_info *limit_table;
	struct gpuppm_status *cur_status;
	int cur_ceiling = -1;
	int cur_floor = g_gpu.opp_num;
	unsigned int cur_c_limiter = LIMIT_NUM;
	unsigned int cur_f_limiter = LIMIT_NUM;
	unsigned int cur_c_priority = GPUPPM_PRIO_NONE;
	unsigned int cur_f_priority = GPUPPM_PRIO_NONE;
	int i = 0;

	GPUFREQ_TRACE_START("target=%d", target);

	if (target == TARGET_GPUSTACK) {
		limit_table = g_gstack_limit_table;
		cur_status = &g_gstack;
	} else {
		limit_table = g_gpu_limit_table;
		cur_status = &g_gpu;
	}

	/* find the largest ceiling */
	for (i = 0; i < LIMIT_NUM; i++) {
		/* skip default value */
		if (limit_table[i].ceiling != GPUPPM_DEFAULT_IDX &&
			limit_table[i].c_enable == LIMIT_ENABLE &&
			limit_table[i].ceiling > cur_ceiling) {
			cur_ceiling = limit_table[i].ceiling;
			cur_c_limiter = limit_table[i].limiter;
			cur_c_priority = limit_table[i].priority;
		}
	}

	/* find the smallest floor */
	for (i = 0; i < LIMIT_NUM; i++) {
		/* skip default value */
		if (limit_table[i].floor != GPUPPM_DEFAULT_IDX &&
			limit_table[i].f_enable == LIMIT_ENABLE &&
			limit_table[i].floor < cur_floor) {
			cur_floor = limit_table[i].floor;
			cur_f_limiter = limit_table[i].limiter;
			cur_f_priority = limit_table[i].priority;
		}
	}

	/* if limit interval with intersection */
	if (cur_ceiling > cur_floor) {
		GPUFREQ_LOGD("exist intersection in limited interval: [%d, %d]",
			cur_ceiling, cur_floor);
		/*
		 * floor_priority > ceiling_priority: align floor
		 * floor_priority < ceiling_priority: align ceiling
		 * floor_priority = ceiling_priority: align ceiling
		 */
		if (cur_f_priority > cur_c_priority)
			cur_ceiling = cur_floor;
		else
			cur_floor = cur_ceiling;
	}

	GPUFREQ_LOGD("[%s ceiling] index: %d, limiter: %d, priority: %d",
		(target == TARGET_GPUSTACK) ? "GPUSTACK" : "GPU",
		cur_ceiling, cur_c_limiter, cur_c_priority);
	GPUFREQ_LOGD("[%s floor] index: %d, limiter: %d, priority: %d",
		(target == TARGET_GPUSTACK) ? "GPUSTACK" : "GPU",
		cur_floor, cur_f_limiter, cur_f_priority);

	cur_status->ceiling = cur_ceiling;
	cur_status->c_limiter = cur_c_limiter;
	cur_status->c_priority = cur_c_priority;
	cur_status->floor = cur_floor;
	cur_status->f_limiter = cur_f_limiter;
	cur_status->f_priority = cur_f_priority;

	GPUFREQ_TRACE_END();
}

int gpuppm_get_ceiling_gpu(void)
{
	return g_gpu.ceiling;
}
EXPORT_SYMBOL(gpuppm_get_ceiling_gpu);

int gpuppm_get_floor_gpu(void)
{
	return g_gpu.floor;
}
EXPORT_SYMBOL(gpuppm_get_floor_gpu);

unsigned int gpuppm_get_c_limiter_gpu(void)
{
	return g_gpu.c_limiter;
}
EXPORT_SYMBOL(gpuppm_get_c_limiter_gpu);

unsigned int gpuppm_get_f_limiter_gpu(void)
{
	return g_gpu.f_limiter;
}
EXPORT_SYMBOL(gpuppm_get_f_limiter_gpu);

const struct gpuppm_limit_info *gpuppm_get_limit_table_gpu(void)
{
	return g_gpu_limit_table;
}
EXPORT_SYMBOL(gpuppm_get_limit_table_gpu);

struct gpufreq_debug_limit_info gpuppm_get_debug_limit_info_gpu(void)
{
	struct gpufreq_debug_limit_info limit_info = {};

	mutex_lock(&gpuppm_lock_gpu);

	limit_info.ceiling = g_gpu.ceiling;
	limit_info.c_limiter = g_gpu.c_limiter;
	limit_info.c_priority = g_gpu.c_priority;
	limit_info.floor = g_gpu.floor;
	limit_info.f_limiter = g_gpu.f_limiter;
	limit_info.f_priority = g_gpu.f_priority;

	mutex_unlock(&gpuppm_lock_gpu);

	return limit_info;
}
EXPORT_SYMBOL(gpuppm_get_debug_limit_info_gpu);

int gpuppm_get_ceiling_gstack(void)
{
	return g_gstack.ceiling;
}
EXPORT_SYMBOL(gpuppm_get_ceiling_gstack);

int gpuppm_get_floor_gstack(void)
{
	return g_gstack.floor;
}
EXPORT_SYMBOL(gpuppm_get_floor_gstack);

unsigned int gpuppm_get_c_limiter_gstack(void)
{
	return g_gstack.c_limiter;
}
EXPORT_SYMBOL(gpuppm_get_c_limiter_gstack);

unsigned int gpuppm_get_f_limiter_gstack(void)
{
	return g_gstack.f_limiter;
}
EXPORT_SYMBOL(gpuppm_get_f_limiter_gstack);

const struct gpuppm_limit_info *gpuppm_get_limit_table_gstack(void)
{
	return g_gstack_limit_table;
}
EXPORT_SYMBOL(gpuppm_get_limit_table_gstack);

struct gpufreq_debug_limit_info gpuppm_get_debug_limit_info_gstack(void)
{
	struct gpufreq_debug_limit_info limit_info = {};

	mutex_lock(&gpuppm_lock_gstack);

	limit_info.ceiling = g_gstack.ceiling;
	limit_info.c_limiter = g_gstack.c_limiter;
	limit_info.c_priority = g_gstack.c_priority;
	limit_info.floor = g_gstack.floor;
	limit_info.f_limiter = g_gstack.f_limiter;
	limit_info.f_priority = g_gstack.f_priority;

	mutex_unlock(&gpuppm_lock_gstack);

	return limit_info;
}
EXPORT_SYMBOL(gpuppm_get_debug_limit_info_gstack);

int gpuppm_set_limit_gpu(
	unsigned int limiter, int ceiling, int floor)
{
	int cur_oppidx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("limiter=%d, ceiling=%d, floor=%d",
		limiter, ceiling, floor);

	if (limiter < 0 || limiter >= LIMIT_NUM) {
		GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	mutex_lock(&gpuppm_lock_gpu);

	if (ceiling == GPUPPM_RESET_IDX || ceiling == GPUPPM_DEFAULT_IDX)
		g_gpu_limit_table[limiter].ceiling = GPUPPM_DEFAULT_IDX;
	else if (ceiling >= 0 && ceiling < g_gpu.opp_num)
		g_gpu_limit_table[limiter].ceiling = ceiling;

	if (floor == GPUPPM_RESET_IDX || floor == GPUPPM_DEFAULT_IDX)
		g_gpu_limit_table[limiter].floor = GPUPPM_DEFAULT_IDX;
	else if (floor >= 0 && floor < g_gpu.opp_num)
		g_gpu_limit_table[limiter].floor = floor;

	/* update current limit status */
	__gpuppm_sort_limit(TARGET_GPU);

	cur_oppidx = __gpufreq_get_cur_idx_gpu();
	cur_ceiling = g_gpu.ceiling;
	cur_floor = g_gpu.floor;

	/* update opp idx if necessary */
	if (cur_oppidx < cur_ceiling)
		ret = __gpufreq_commit_gpu(cur_ceiling, DVFS_FREE);
	else if (cur_oppidx > cur_floor)
		ret = __gpufreq_commit_gpu(cur_floor, DVFS_FREE);

	mutex_unlock(&gpuppm_lock_gpu);

done:
	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpuppm_set_limit_gpu);

int gpuppm_switch_limit_gpu(
	unsigned int limiter, int c_enable, int f_enable)
{
	int cur_oppidx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ret = GPUFREQ_SUCCESS;

	if (limiter < 0 || limiter >= LIMIT_NUM) {
		GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	mutex_lock(&gpuppm_lock_gpu);

	if (c_enable == LIMIT_ENABLE)
		g_gpu_limit_table[limiter].c_enable = LIMIT_ENABLE;
	else if (c_enable == LIMIT_DISABLE)
		g_gpu_limit_table[limiter].c_enable = LIMIT_DISABLE;

	if (f_enable == LIMIT_ENABLE)
		g_gpu_limit_table[limiter].f_enable = LIMIT_ENABLE;
	else if (f_enable == LIMIT_DISABLE)
		g_gpu_limit_table[limiter].f_enable = LIMIT_DISABLE;

	/* update current limit status */
	__gpuppm_sort_limit(TARGET_GPU);

	cur_oppidx = __gpufreq_get_cur_idx_gpu();
	cur_ceiling = g_gpu.ceiling;
	cur_floor = g_gpu.floor;

	/* update opp idx if necessary */
	if (cur_oppidx < cur_ceiling)
		ret = __gpufreq_commit_gpu(cur_ceiling, DVFS_FREE);
	else if (cur_oppidx > cur_floor)
		ret = __gpufreq_commit_gpu(cur_floor, DVFS_FREE);

	mutex_unlock(&gpuppm_lock_gpu);

done:
	return ret;
}
EXPORT_SYMBOL(gpuppm_switch_limit_gpu);

int gpuppm_set_limit_gstack(
	unsigned int limiter, int ceiling, int floor)
{
	return GPUFREQ_SUCCESS;
}
EXPORT_SYMBOL(gpuppm_set_limit_gstack);

int gpuppm_switch_limit_gstack(
	unsigned int limiter, int c_enable, int f_enable)
{
	int cur_oppidx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ret = GPUFREQ_SUCCESS;

	if (limiter < 0 || limiter >= LIMIT_NUM) {
		GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
		ret = GPUFREQ_EINVAL;
		goto done;
	}

	mutex_lock(&gpuppm_lock_gstack);

	if (c_enable == LIMIT_ENABLE)
		g_gstack_limit_table[limiter].c_enable = LIMIT_ENABLE;
	else if (c_enable == LIMIT_DISABLE)
		g_gstack_limit_table[limiter].c_enable = LIMIT_DISABLE;

	if (f_enable == LIMIT_ENABLE)
		g_gstack_limit_table[limiter].f_enable = LIMIT_ENABLE;
	else if (f_enable == LIMIT_DISABLE)
		g_gstack_limit_table[limiter].f_enable = LIMIT_DISABLE;

	/* update current limit status */
	__gpuppm_sort_limit(TARGET_GPUSTACK);

	cur_oppidx = __gpufreq_get_cur_idx_gstack();
	cur_ceiling = g_gstack.ceiling;
	cur_floor = g_gstack.floor;

	/* update opp idx if necessary */
	if (cur_oppidx < cur_ceiling)
		ret = __gpufreq_commit_gstack(cur_ceiling, DVFS_FREE);
	else if (cur_oppidx > cur_floor)
		ret = __gpufreq_commit_gstack(cur_floor, DVFS_FREE);

	mutex_unlock(&gpuppm_lock_gstack);

done:
	return ret;
}
EXPORT_SYMBOL(gpuppm_switch_limit_gstack);

int gpuppm_limited_commit_gpu(int oppidx)
{
	int limited_idx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("oppidx=%d", oppidx);

	mutex_lock(&gpuppm_lock_gpu);

	/* fit to limited interval */
	cur_ceiling = g_gpu.ceiling;
	cur_floor = g_gpu.floor;
	if (oppidx < cur_ceiling)
		limited_idx = cur_ceiling;
	else if (oppidx > cur_floor)
		limited_idx = cur_floor;
	else
		limited_idx = oppidx;

	GPUFREQ_LOGD("restrict OPP index: (%d->%d), limited interval: [%d, %d]",
		oppidx, limited_idx, cur_ceiling, cur_floor);

	ret = __gpufreq_commit_gpu(limited_idx, DVFS_FREE);

	mutex_unlock(&gpuppm_lock_gpu);

	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpuppm_limited_commit_gpu);

int gpuppm_limited_commit_gstack(int oppidx)
{
	int limited_idx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("oppidx=%d", oppidx);

	ret = __gpufreq_commit_gstack(limited_idx, DVFS_FREE);

	GPUFREQ_TRACE_END();

	return ret;
}
EXPORT_SYMBOL(gpuppm_limited_commit_gstack);

int gpuppm_init(void)
{
	int max_oppidx_gpu = 0, min_oppidx_gpu = 0;
	unsigned int opp_num_gpu = 0;
	int ret = GPUFREQ_SUCCESS;

	opp_num_gpu = __gpufreq_get_opp_num_gpu();
	max_oppidx_gpu = __gpufreq_get_max_idx_gpu();
	min_oppidx_gpu = __gpufreq_get_min_idx_gpu();

	g_gpu.opp_num = opp_num_gpu;
	gpuppm_set_limit_gpu(LIMIT_SEGMENT, max_oppidx_gpu, min_oppidx_gpu);

	return ret;
}
