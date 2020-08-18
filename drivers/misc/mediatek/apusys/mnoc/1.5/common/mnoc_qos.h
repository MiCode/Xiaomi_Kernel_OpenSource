/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MNOC_QOS_H__
#define __APUSYS_MNOC_QOS_H__

#include "mnoc_option.h"
#include <linux/device.h>
#include <linux/pm_qos.h>

enum apu_qos_engine {
	APU_QOS_ENGINE_VPU0,
	APU_QOS_ENGINE_VPU1,
	APU_QOS_ENGINE_MDLA0,
	APU_QOS_ENGINE_EDMA0,
	APU_QOS_ENGINE_MD32,

	NR_APU_QOS_ENGINE
};


struct engine_pm_qos_counter {
	struct pm_qos_request qos_req;
	struct icc_path *emi_icc_path;

	int32_t last_report_bw;
	unsigned int last_idx;
	unsigned int core;
};

#if MNOC_TIME_PROFILE
extern unsigned long sum_start, sum_suspend, sum_end, sum_work_func;
extern unsigned int cnt_start, cnt_suspend, cnt_end, cnt_work_func;
#endif

#if MNOC_QOS_BOOST_ENABLE
extern bool apu_qos_boost_flag;
extern struct mutex apu_qos_boost_mtx;
#endif

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
void apu_qos_on(void);
void apu_qos_off(void);

void apu_qos_counter_init(struct device *dev);
void apu_qos_counter_destroy(struct device *dev);

void print_cmd_qos_list(struct seq_file *m);

void apu_qos_suspend(void);
void apu_qos_resume(void);

void apu_qos_boost_start(void);
void apu_qos_boost_end(void);

#else /* !ENABLED CONFIG_MTK_QOS_FRAMEWORK */
static inline void apu_qos_on(void) {}
static inline void apu_qos_off(void) {}

static inline void apu_qos_counter_init(struct device *dev) {}
static inline void apu_qos_counter_destroy(struct device *dev) {}

static inline void print_cmd_qos_list(struct seq_file *m) {}

static inline void apu_qos_suspend(void) {}
static inline void apu_qos_resume(void) {}

static inline void apu_qos_boost_start(void) {}
static inline void apu_qos_boost_end(void) {}

#endif
#endif
