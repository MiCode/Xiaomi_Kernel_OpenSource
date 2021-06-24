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

#include <gpufreq_v2.h>
#include <gpufreq_debug.h>
#include <gpuppm.h>
#include <gpufreq_common.h>

/**
 * ===============================================
 * Local Function Declaration
 * ===============================================
 */
static void __gpuppm_sort_limit(enum gpufreq_target target);
static int __gpuppm_covert_limit_to_idx(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int ceiling_info, int floor_info, int *ceiling_idx, int *floor_idx);

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static DEFINE_MUTEX(gpuppm_lock);
static struct gpuppm_status g_gpu;
static struct gpuppm_status g_stack;
unsigned int g_gpueb_support;

static struct gpuppm_limit_info g_gpu_limit_table[] = {
	LIMITOP(LIMIT_SEGMENT, "SEGMENT", GPUPPM_PRIO_9,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_DEBUG, "DEBUG", GPUPPM_PRIO_8,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_THERMAL_AP, "THERMAL_AP", GPUPPM_PRIO_7,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_THERMAL_EB, "THERMAL_EB", GPUPPM_PRIO_7,
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

static struct gpuppm_limit_info g_stack_limit_table[] = {
	LIMITOP(LIMIT_SEGMENT, "SEGMENT", GPUPPM_PRIO_9,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_DEBUG, "DEBUG", GPUPPM_PRIO_8,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_THERMAL_AP, "THERMAL_AP", GPUPPM_PRIO_7,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE,
		GPUPPM_DEFAULT_IDX, LIMIT_ENABLE),
	LIMITOP(LIMIT_THERMAL_EB, "THERMAL_EB", GPUPPM_PRIO_7,
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

static struct gpuppm_platform_fp platform_fp = {
	.limited_commit_gpu = gpuppm_limited_commit_gpu,
	.limited_commit_stack = gpuppm_limited_commit_stack,
	.set_limit_gpu = gpuppm_set_limit_gpu,
	.switch_limit_gpu = gpuppm_switch_limit_gpu,
	.set_limit_stack = gpuppm_set_limit_stack,
	.switch_limit_stack = gpuppm_switch_limit_stack,
	.get_ceiling_gpu = gpuppm_get_ceiling_gpu,
	.get_floor_gpu = gpuppm_get_floor_gpu,
	.get_c_limiter_gpu = gpuppm_get_c_limiter_gpu,
	.get_f_limiter_gpu = gpuppm_get_f_limiter_gpu,
	.get_limit_table_gpu = gpuppm_get_limit_table_gpu,
	.get_debug_limit_info_gpu = gpuppm_get_debug_limit_info_gpu,
	.get_ceiling_stack = gpuppm_get_ceiling_stack,
	.get_floor_stack = gpuppm_get_floor_stack,
	.get_c_limiter_stack = gpuppm_get_c_limiter_stack,
	.get_f_limiter_stack = gpuppm_get_f_limiter_stack,
	.get_limit_table_stack = gpuppm_get_limit_table_stack,
	.get_debug_limit_info_stack = gpuppm_get_debug_limit_info_stack,
};

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
static void __gpuppm_sort_limit(enum gpufreq_target target)
{
	struct gpuppm_limit_info *limit_table;
	struct gpuppm_status *cur_status;
	int cur_ceiling = 0, cur_floor = 0;
	unsigned int cur_c_limiter = LIMIT_NUM;
	unsigned int cur_f_limiter = LIMIT_NUM;
	unsigned int cur_c_priority = GPUPPM_PRIO_NONE;
	unsigned int cur_f_priority = GPUPPM_PRIO_NONE;
	int i = 0;

	GPUFREQ_TRACE_START("target=%d", target);

	if (target == TARGET_STACK) {
		limit_table = g_stack_limit_table;
		cur_status = &g_stack;
		cur_ceiling = -1;
		cur_floor = g_stack.opp_num;
	} else {
		limit_table = g_gpu_limit_table;
		cur_status = &g_gpu;
		cur_ceiling = -1;
		cur_floor = g_gpu.opp_num;
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

	/* only update if both limiter of ceiling/floor are found */
	if (cur_c_limiter != LIMIT_NUM && cur_f_limiter != LIMIT_NUM) {
		GPUFREQ_LOGD("[%s ceiling] index: %d, limiter: %s, priority: %d",
			(target == TARGET_STACK) ? "STACK" : "GPU",
			cur_ceiling, limit_table[cur_c_limiter].name, cur_c_priority);
		GPUFREQ_LOGD("[%s floor] index: %d, limiter: %s, priority: %d",
			(target == TARGET_STACK) ? "STACK" : "GPU",
			cur_floor, limit_table[cur_f_limiter].name, cur_f_priority);

		cur_status->ceiling = cur_ceiling;
		cur_status->c_limiter = cur_c_limiter;
		cur_status->c_priority = cur_c_priority;
		cur_status->floor = cur_floor;
		cur_status->f_limiter = cur_f_limiter;
		cur_status->f_priority = cur_f_priority;
	}

	GPUFREQ_TRACE_END();
}

static int __gpuppm_covert_limit_to_idx(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int ceiling_info, int floor_info, int *ceiling_idx, int *floor_idx)
{
	int ret = GPUFREQ_SUCCESS;

	switch (limiter)
	{
		case LIMIT_SEGMENT:
			/* limit info: OPP index */
			*ceiling_idx = ceiling_info;
			*floor_idx = floor_info;
			break;
		case LIMIT_DEBUG:
			/* limit info: OPP index */
			*ceiling_idx = ceiling_info;
			*floor_idx = floor_info;
			break;
		case LIMIT_THERMAL_AP:
			/* limit info: freq */
			if (ceiling_info > 0)
				if (target == TARGET_STACK)
					*ceiling_idx = __gpufreq_get_idx_by_fstack(
						(unsigned int)ceiling_info);
				else
					*ceiling_idx = __gpufreq_get_idx_by_fgpu(
						(unsigned int)ceiling_info);
			else
				*ceiling_idx = GPUPPM_RESET_IDX;
			*floor_idx = GPUPPM_KEEP_IDX;
			break;
		case LIMIT_THERMAL_EB:
			/* limit info: power */
			if (ceiling_info > 0)
				if (target == TARGET_STACK)
					*ceiling_idx = __gpufreq_get_idx_by_pstack(
						(unsigned int)ceiling_info);
				else
					*ceiling_idx = __gpufreq_get_idx_by_pgpu(
						(unsigned int)ceiling_info);
			else
				*ceiling_idx = GPUPPM_RESET_IDX;
			*floor_idx = GPUPPM_KEEP_IDX;
			break;
		case LIMIT_SRAMRC:
			/* limit info: volt */
			*ceiling_idx = GPUPPM_KEEP_IDX;
			if (floor_info > 0)
				if (target == TARGET_STACK)
					*floor_idx = __gpufreq_get_idx_by_vstack(
						(unsigned int)floor_info);
				else
					*floor_idx = __gpufreq_get_idx_by_vgpu(
						(unsigned int)floor_info);
			else
				*floor_idx = GPUPPM_RESET_IDX;
			break;
		case LIMIT_BATT_OC:
			/* limit info: batt_oc_level */
			*ceiling_idx = __gpufreq_get_batt_oc_idx(ceiling_info);
			*floor_idx = GPUPPM_KEEP_IDX;
			break;
		case LIMIT_BATT_PERCENT:
			/* limit info: batt_percent_level */
			*ceiling_idx = __gpufreq_get_batt_percent_idx(ceiling_info);
			*floor_idx = GPUPPM_KEEP_IDX;
			break;
		case LIMIT_LOW_BATT:
			/* limit info: low_batt_level */
			*ceiling_idx = __gpufreq_get_low_batt_idx(ceiling_info);
			*floor_idx = GPUPPM_KEEP_IDX;
			break;
		case LIMIT_PBM:
			/* limit info: power */
			if (ceiling_info > 0)
				if (target == TARGET_STACK)
					*ceiling_idx = __gpufreq_get_idx_by_pstack(
						(unsigned int)ceiling_info);
				else
					*ceiling_idx = __gpufreq_get_idx_by_pgpu(
						(unsigned int)ceiling_info);
			else
				*ceiling_idx = GPUPPM_RESET_IDX;
			*floor_idx = GPUPPM_KEEP_IDX;
			break;
		case LIMIT_APIBOOST:
			/* limit info: OPP index */
			*ceiling_idx = ceiling_info;
			*floor_idx = floor_info;
			break;
		case LIMIT_FPSGO:
			/* limit info: OPP index */
			*ceiling_idx = ceiling_info;
			*floor_idx = floor_info;
			break;
		default:
			GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
			ret = GPUFREQ_EINVAL;
			break;
	}

	GPUFREQ_LOGD("[%s limiter: %d] ceiling_info: %d (idx: %d), floor_info: %d, (idx: %d)",
		(target == TARGET_STACK) ? "STACK" : "GPU",
		limiter, ceiling_info, *ceiling_idx, floor_info, *floor_idx);

	return ret;
}

int gpuppm_get_ceiling_gpu(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return -1;
	/* AP mode */
	else
		return g_gpu.ceiling;
}

int gpuppm_get_floor_gpu(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return -1;
	/* AP mode */
	else
		return g_gpu.floor;
}

unsigned int gpuppm_get_c_limiter_gpu(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return 0;
	/* AP mode */
	else
		return g_gpu.c_limiter;
}

unsigned int gpuppm_get_f_limiter_gpu(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return 0;
	/* AP mode */
	else
		return g_gpu.f_limiter;
}

const struct gpuppm_limit_info *gpuppm_get_limit_table_gpu(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return NULL;
	/* AP mode */
	else
		return g_gpu_limit_table;
}

struct gpufreq_debug_limit_info gpuppm_get_debug_limit_info_gpu(void)
{
	struct gpufreq_debug_limit_info limit_info = {};

	/* GPUEB mode */
	if (g_gpueb_support) {
		/* gpueb_ipi */
		GPUFREQ_UNREFERENCED(limit_info);
	/* AP mode */
	} else {
		mutex_lock(&gpuppm_lock);

		limit_info.ceiling = g_gpu.ceiling;
		limit_info.c_limiter = g_gpu.c_limiter;
		limit_info.c_priority = g_gpu.c_priority;
		limit_info.floor = g_gpu.floor;
		limit_info.f_limiter = g_gpu.f_limiter;
		limit_info.f_priority = g_gpu.f_priority;

		mutex_unlock(&gpuppm_lock);
	}

	return limit_info;
}

int gpuppm_get_ceiling_stack(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return -1;
	else
		return g_stack.ceiling;
}

int gpuppm_get_floor_stack(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return -1;
	/* AP mode */
	else
		return g_stack.floor;
}

unsigned int gpuppm_get_c_limiter_stack(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return 0;
	/* AP mode */
	else
		return g_stack.c_limiter;
}

unsigned int gpuppm_get_f_limiter_stack(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return 0;
	/* AP mode */
	else
		return g_stack.f_limiter;
}

const struct gpuppm_limit_info *gpuppm_get_limit_table_stack(void)
{
	/* GPUEB mode */
	if (g_gpueb_support)
		/* gpueb_ipi */
		return NULL;
	/* AP mode */
	else
		return g_stack_limit_table;
}

struct gpufreq_debug_limit_info gpuppm_get_debug_limit_info_stack(void)
{
	struct gpufreq_debug_limit_info limit_info = {};

	/* GPUEB mode */
	if (g_gpueb_support) {
		/* gpueb_ipi */
		GPUFREQ_UNREFERENCED(limit_info);
	/* AP mode */
	} else {
		mutex_lock(&gpuppm_lock);

		limit_info.ceiling = g_stack.ceiling;
		limit_info.c_limiter = g_stack.c_limiter;
		limit_info.c_priority = g_stack.c_priority;
		limit_info.floor = g_stack.floor;
		limit_info.f_limiter = g_stack.f_limiter;
		limit_info.f_priority = g_stack.f_priority;

		mutex_unlock(&gpuppm_lock);
	}

	return limit_info;
}

int gpuppm_set_limit_gpu(enum gpuppm_limiter limiter, int ceiling_info, int floor_info)
{
	int cur_oppidx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ceiling_idx = 0, floor_idx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("limiter=%d, ceiling_info=%d, floor_info=%d",
		limiter, ceiling_info, floor_info);

	/* GPUEB mode */
	if (g_gpueb_support) {
		/* gpueb_ipi */
		GPUFREQ_UNREFERENCED(limiter);
		GPUFREQ_UNREFERENCED(ceiling_info);
		GPUFREQ_UNREFERENCED(floor_info);
		ret = GPUFREQ_SUCCESS;
	/* AP mode */
	} else {
		if (limiter < 0 || limiter >= LIMIT_NUM) {
			GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
			ret = GPUFREQ_EINVAL;
			goto done;
		}

		mutex_lock(&gpuppm_lock);

		/* covert input limit info to OPP index */
		ret = __gpuppm_covert_limit_to_idx(TARGET_GPU, limiter,
			ceiling_info, floor_info, &ceiling_idx, &floor_idx);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to covert limit info to OPP index (%d)", ret);
			goto done;
		}

		if (ceiling_idx == GPUPPM_RESET_IDX || ceiling_idx == GPUPPM_DEFAULT_IDX)
			g_gpu_limit_table[limiter].ceiling = GPUPPM_DEFAULT_IDX;
		else if (ceiling_idx >= 0 && ceiling_idx < g_gpu.opp_num)
			g_gpu_limit_table[limiter].ceiling = ceiling_idx;

		if (floor_idx == GPUPPM_RESET_IDX || floor_idx == GPUPPM_DEFAULT_IDX)
			g_gpu_limit_table[limiter].floor = GPUPPM_DEFAULT_IDX;
		else if (floor_idx >= 0 && floor_idx < g_gpu.opp_num)
			g_gpu_limit_table[limiter].floor = floor_idx;

		/* update current limit status */
		__gpuppm_sort_limit(TARGET_GPU);

		cur_oppidx = __gpufreq_get_cur_idx_gpu();
		cur_ceiling = g_gpu.ceiling;
		cur_floor = g_gpu.floor;

		/* update opp idx if necessary */
		if (cur_oppidx < cur_ceiling)
			ret = __gpufreq_generic_commit_gpu(cur_ceiling, DVFS_FREE);
		else if (cur_oppidx > cur_floor)
			ret = __gpufreq_generic_commit_gpu(cur_floor, DVFS_FREE);

		mutex_unlock(&gpuppm_lock);
	}

done:
	GPUFREQ_TRACE_END();

	return ret;
}

int gpuppm_switch_limit_gpu(enum gpuppm_limiter limiter, int c_enable, int f_enable)
{
	int cur_oppidx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ret = GPUFREQ_SUCCESS;

	/* GPUEB mode */
	if (g_gpueb_support) {
		/* gpueb_ipi */
		GPUFREQ_UNREFERENCED(limiter);
		GPUFREQ_UNREFERENCED(c_enable);
		GPUFREQ_UNREFERENCED(f_enable);
		ret = GPUFREQ_SUCCESS;
	/* AP mode */
	} else {
		if (limiter < 0 || limiter >= LIMIT_NUM) {
			GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
			ret = GPUFREQ_EINVAL;
			goto done;
		}

		mutex_lock(&gpuppm_lock);

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
			ret = __gpufreq_generic_commit_gpu(cur_ceiling, DVFS_FREE);
		else if (cur_oppidx > cur_floor)
			ret = __gpufreq_generic_commit_gpu(cur_floor, DVFS_FREE);

		mutex_unlock(&gpuppm_lock);
	}

done:
	return ret;
}

int gpuppm_set_limit_stack(enum gpuppm_limiter limiter, int ceiling_info, int floor_info)
{
	int cur_oppidx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ceiling_idx = 0, floor_idx = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("limiter=%d, ceiling=%d, floor_info=%d",
		limiter, ceiling_info, floor_info);

	/* GPUEB mode */
	if (g_gpueb_support) {
		/* gpueb_ipi */
		GPUFREQ_UNREFERENCED(limiter);
		GPUFREQ_UNREFERENCED(ceiling_info);
		GPUFREQ_UNREFERENCED(floor_info);
		ret = GPUFREQ_SUCCESS;
	/* AP mode */
	} else {
		if (limiter < 0 || limiter >= LIMIT_NUM) {
			GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
			ret = GPUFREQ_EINVAL;
			goto done;
		}

		mutex_lock(&gpuppm_lock);

		/* covert input limit info to OPP index */
		ret = __gpuppm_covert_limit_to_idx(TARGET_STACK, limiter,
			ceiling_info, floor_info, &ceiling_idx, &floor_idx);
		if (unlikely(ret)) {
			GPUFREQ_LOGE("fail to covert limit info to OPP index (%d)", ret);
			goto done;
		}

		if (ceiling_idx == GPUPPM_RESET_IDX || ceiling_idx == GPUPPM_DEFAULT_IDX)
			g_stack_limit_table[limiter].ceiling = GPUPPM_DEFAULT_IDX;
		else if (ceiling_idx >= 0 && ceiling_idx < g_stack.opp_num)
			g_stack_limit_table[limiter].ceiling = ceiling_idx;

		if (floor_idx == GPUPPM_RESET_IDX || floor_idx == GPUPPM_DEFAULT_IDX)
			g_stack_limit_table[limiter].floor = GPUPPM_DEFAULT_IDX;
		else if (floor_idx >= 0 && floor_idx < g_stack.opp_num)
			g_stack_limit_table[limiter].floor = floor_idx;

		/* update current limit status */
		__gpuppm_sort_limit(TARGET_STACK);

		cur_oppidx = __gpufreq_get_cur_idx_stack();
		cur_ceiling = g_stack.ceiling;
		cur_floor = g_stack.floor;

		/* update opp idx if necessary */
		if (cur_oppidx < cur_ceiling)
			ret = __gpufreq_generic_commit_stack(cur_ceiling, DVFS_FREE);
		else if (cur_oppidx > cur_floor)
			ret = __gpufreq_generic_commit_stack(cur_floor, DVFS_FREE);

		mutex_unlock(&gpuppm_lock);
	}
done:
	GPUFREQ_TRACE_END();

	return ret;
}

int gpuppm_switch_limit_stack(enum gpuppm_limiter limiter, int c_enable, int f_enable)
{
	int cur_oppidx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ret = GPUFREQ_SUCCESS;

	/* GPUEB mode */
	if (g_gpueb_support) {
		/* gpueb_ipi */
		GPUFREQ_UNREFERENCED(limiter);
		GPUFREQ_UNREFERENCED(c_enable);
		GPUFREQ_UNREFERENCED(f_enable);
		ret = GPUFREQ_SUCCESS;
	/* AP mode */
	} else {
		if (limiter < 0 || limiter >= LIMIT_NUM) {
			GPUFREQ_LOGE("invalid limiter: %d (EINVAL)", limiter);
			ret = GPUFREQ_EINVAL;
			goto done;
		}

		mutex_lock(&gpuppm_lock);

		if (c_enable == LIMIT_ENABLE)
			g_stack_limit_table[limiter].c_enable = LIMIT_ENABLE;
		else if (c_enable == LIMIT_DISABLE)
			g_stack_limit_table[limiter].c_enable = LIMIT_DISABLE;

		if (f_enable == LIMIT_ENABLE)
			g_stack_limit_table[limiter].f_enable = LIMIT_ENABLE;
		else if (f_enable == LIMIT_DISABLE)
			g_stack_limit_table[limiter].f_enable = LIMIT_DISABLE;

		/* update current limit status */
		__gpuppm_sort_limit(TARGET_STACK);

		cur_oppidx = __gpufreq_get_cur_idx_stack();
		cur_ceiling = g_stack.ceiling;
		cur_floor = g_stack.floor;

		/* update opp idx if necessary */
		if (cur_oppidx < cur_ceiling)
			ret = __gpufreq_generic_commit_stack(cur_ceiling, DVFS_FREE);
		else if (cur_oppidx > cur_floor)
			ret = __gpufreq_generic_commit_stack(cur_floor, DVFS_FREE);

		mutex_unlock(&gpuppm_lock);
	}

done:
	return ret;
}

int gpuppm_limited_commit_gpu(int oppidx)
{
	int limited_idx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("oppidx=%d", oppidx);

	/* GPUEB mode */
	if (g_gpueb_support) {
		/* gpueb_ipi */
		GPUFREQ_UNREFERENCED(oppidx);
		ret = GPUFREQ_SUCCESS;
	/* AP mode */
	} else {
		mutex_lock(&gpuppm_lock);

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

		ret = __gpufreq_generic_commit_gpu(limited_idx, DVFS_FREE);

		mutex_unlock(&gpuppm_lock);
	}

	GPUFREQ_TRACE_END();

	return ret;
}

int gpuppm_limited_commit_stack(int oppidx)
{
	int limited_idx = 0;
	int cur_ceiling = 0, cur_floor = 0;
	int ret = GPUFREQ_SUCCESS;

	GPUFREQ_TRACE_START("oppidx=%d", oppidx);

	/* GPUEB mode */
	if (g_gpueb_support) {
		/* gpueb_ipi */
		GPUFREQ_UNREFERENCED(oppidx);
		ret = GPUFREQ_SUCCESS;
	/* AP mode */
	} else {
		mutex_lock(&gpuppm_lock);

		/* fit to limited interval */
		cur_ceiling = g_stack.ceiling;
		cur_floor = g_stack.floor;
		if (oppidx < cur_ceiling)
			limited_idx = cur_ceiling;
		else if (oppidx > cur_floor)
			limited_idx = cur_floor;
		else
			limited_idx = oppidx;

		GPUFREQ_LOGD("restrict OPP index: (%d->%d), limited interval: [%d, %d]",
			oppidx, limited_idx, cur_ceiling, cur_floor);

		ret = __gpufreq_generic_commit_stack(limited_idx, DVFS_FREE);

		mutex_unlock(&gpuppm_lock);
	}

	GPUFREQ_TRACE_END();

	return ret;
}

int gpuppm_init(unsigned int gpueb_support)
{
	int max_oppidx_gpu = 0, min_oppidx_gpu = 0;
	int max_oppidx_stack = 0, min_oppidx_stack = 0;
	unsigned int opp_num_gpu = 0, opp_num_stack = 0;
	int ret = GPUFREQ_SUCCESS;

	g_gpueb_support = gpueb_support;

	/* register gpuppm function to wrapper in both AP and EB mode */
	gpufreq_register_gpuppm_fp(&platform_fp);
	gpufreq_debug_register_gpuppm_fp(&platform_fp);

	/* init only in AP mode */
	if (!g_gpueb_support) {
		opp_num_gpu = __gpufreq_get_opp_num_gpu();
		max_oppidx_gpu = __gpufreq_get_max_idx_gpu();
		min_oppidx_gpu = __gpufreq_get_min_idx_gpu();

		opp_num_stack = __gpufreq_get_opp_num_stack();
		max_oppidx_stack = __gpufreq_get_max_idx_stack();
		min_oppidx_stack = __gpufreq_get_min_idx_stack();

		g_gpu.opp_num = opp_num_gpu;
		g_stack.opp_num = opp_num_stack;

		gpuppm_set_limit_gpu(LIMIT_SEGMENT, max_oppidx_gpu, min_oppidx_gpu);
		gpuppm_set_limit_stack(LIMIT_SEGMENT, max_oppidx_stack, min_oppidx_stack);
	}

	return ret;
}
