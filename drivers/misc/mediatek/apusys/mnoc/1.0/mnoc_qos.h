// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APUSYS_MNOC_QOS_H__
#define __APUSYS_MNOC_QOS_H__

#include "mnoc_option.h"

#if MNOC_TIME_PROFILE
extern unsigned long sum_start, sum_suspend, sum_end, sum_work_func;
extern unsigned int cnt_start, cnt_suspend, cnt_end, cnt_work_func;
#endif

#if MNOC_QOS_BOOST_ENABLE
extern bool apu_qos_boost_flag;
extern struct mutex apu_qos_boost_mtx;
#endif

void apu_qos_on(void);
void apu_qos_off(void);

void apu_qos_counter_init(struct device *dev);
void apu_qos_counter_destroy(struct device *dev);

void print_cmd_qos_list(struct seq_file *m);

void apu_qos_suspend(void);
void apu_qos_resume(void);

void apu_qos_boost_start(void);
void apu_qos_boost_end(void);

#endif
