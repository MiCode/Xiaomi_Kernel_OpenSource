/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUPPM_H__
#define __GPUPPM_H__

/**************************************************
 * Definition
 **************************************************/
#define LIMITOP(_limiter, _name, _priority, _ceiling, _c_en, _floor, _f_en) \
	{                                      \
		.limiter = _limiter,               \
		.name = _name,                     \
		.priority = _priority,             \
		.ceiling = _ceiling,               \
		.c_enable = _c_en,                 \
		.floor = _floor,                   \
		.f_enable = _f_en,                 \
	}

/**************************************************
 * Enumeration
 **************************************************/
enum gpuppm_priority {
	GPUPPM_PRIO_NONE = 0, /* the lowest priority */
	GPUPPM_PRIO_1 = 1,
	GPUPPM_PRIO_2,
	GPUPPM_PRIO_3,
	GPUPPM_PRIO_4,
	GPUPPM_PRIO_5,
	GPUPPM_PRIO_6,
	GPUPPM_PRIO_7,
	GPUPPM_PRIO_8,
	GPUPPM_PRIO_9, /* the highest priority */
};

enum gpuppm_limit_state {
	LIMIT_DISABLE = 0,
	LIMIT_ENABLE,
};

/**************************************************
 * Structure
 **************************************************/
struct gpuppm_status {
	int ceiling;
	unsigned int c_limiter;
	unsigned int c_priority;
	int floor;
	unsigned int f_limiter;
	unsigned int f_priority;
	int opp_num;
};

/**************************************************
 * Function
 **************************************************/
int gpuppm_init(enum gpufreq_target target, unsigned int gpueb_support, unsigned int sramrc_vsafe);
int gpuppm_limited_commit(enum gpufreq_target target, int oppidx);
int gpuppm_set_limit(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int ceiling_info, int floor_info);
int gpuppm_switch_limit(enum gpufreq_target target, enum gpuppm_limiter limiter,
	int c_enable, int f_enable);
int gpuppm_get_ceiling(enum gpufreq_target target);
int gpuppm_get_floor(enum gpufreq_target target);
unsigned int gpuppm_get_c_limiter(enum gpufreq_target target);
unsigned int gpuppm_get_f_limiter(enum gpufreq_target target);
const struct gpuppm_limit_info *gpuppm_get_limit_table(enum gpufreq_target target);
struct gpufreq_debug_limit_info gpuppm_get_debug_limit_info(enum gpufreq_target target);

#endif /* __GPUPPM_H__ */
