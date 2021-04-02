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
	unsigned int opp_num;
};

struct gpuppm_limit_info {
	unsigned int limiter;
	char *name;
	unsigned int priority;
	int ceiling;
	unsigned int c_enable;
	int floor;
	unsigned int f_enable;
};

/**************************************************
 * Function
 **************************************************/
int gpuppm_init(void);
int gpuppm_limited_commit_gpu(int oppidx);
int gpuppm_limited_commit_gstack(int oppidx);
int gpuppm_set_limit_gpu(
	unsigned int limiter, int ceiling, int floor);
int gpuppm_switch_limit_gpu(
	unsigned int limiter, int c_enable, int f_enable);
int gpuppm_set_limit_gstack(
	unsigned int limiter, int ceiling, int floor);
int gpuppm_switch_limit_gstack(
	unsigned int limiter, int c_enable, int f_enable);
int gpuppm_get_ceiling_gpu(void);
int gpuppm_get_floor_gpu(void);
unsigned int gpuppm_get_c_limiter_gpu(void);
unsigned int gpuppm_get_f_limiter_gpu(void);
const struct gpuppm_limit_info *gpuppm_get_limit_table_gpu(void);
struct gpufreq_debug_limit_info gpuppm_get_debug_limit_info_gpu(void);
int gpuppm_get_ceiling_gstack(void);
int gpuppm_get_floor_gstack(void);
unsigned int gpuppm_get_c_limiter_gstack(void);
unsigned int gpuppm_get_f_limiter_gstack(void);
const struct gpuppm_limit_info *gpuppm_get_limit_table_gstack(void);
struct gpufreq_debug_limit_info gpuppm_get_debug_limit_info_gstack(void);

#endif /* __GPUPPM_H__ */
