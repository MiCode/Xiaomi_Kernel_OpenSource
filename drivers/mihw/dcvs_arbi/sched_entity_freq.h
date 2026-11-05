/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2025, Xiaomi Inc. All rights reserved.
 */

#ifndef _SCHED_ENTITY_FREQ_H_
#define _SCHED_ENTITY_FREQ_H_

#include <linux/types.h>
#include <linux/cpumask.h>

/* 调度实体调频控制配置 */
struct sched_entity_config {
	/* CPU级别的run queue计数器阈值 */
	unsigned int idle_threshold;		/* 单个CPU触发省电动作的阈值 */
	unsigned int active_threshold;		/* 单个CPU退出省电动作的阈值 */
	unsigned int cluster_idle_threshold;	/* Cluster触发省电的活跃CPU阈值 */
	unsigned int cluster_active_threshold;	/* Cluster退出省电的活跃CPU阈值 */

	/* 调频参数 */
	unsigned int min_freq_khz;		/* 最低频率(kHz) */
	unsigned int mid_freq_khz;		/* 中间频率(kHz) - 分级降频 */
	unsigned int normal_freq_khz;		/* 正常频率(kHz) */
	unsigned int fast_switch_delay_ms;	/* 快速切换延迟(ms) */

	/* DDR频率联动参数 */
	bool ddr_freq_sync;			/* 是否同步DDR频率 */
	unsigned int ddr_min_freq_khz;		/* DDR最低频率(kHz) */

	/* 功能控制 */
	bool enable;				/* 功能使能 */
};

/* 外部接口函数 - 功能控制 */
void sched_entity_freq_enable(bool enable);

/* config mutex - 用于 sysfs store 函数保护 */
extern struct mutex config_mutex;

/* 状态查询接口 */
bool sched_entity_freq_is_power_save_mode(unsigned int cluster_id);
unsigned int sched_entity_freq_get_runq_count(int cpu);

/* Sysfs接口 - 内部使用 */
int sched_entity_freq_sysfs_init(void);
void sched_entity_freq_sysfs_exit(void);

#endif /* _SCHED_ENTITY_FREQ_H_ */
