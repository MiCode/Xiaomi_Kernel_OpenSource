/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __MTK_CPUFREQ_DEBUG_H__
#define __MTK_CPUFREQ_DEBUG_H__

#include "mtk_cpufreq_internal.h"

extern void aee_record_cpu_dvfs_in(struct mt_cpu_dvfs *p);
extern void aee_record_cpu_dvfs_out(struct mt_cpu_dvfs *p);
extern void aee_record_cpu_dvfs_step(unsigned int step);
extern void aee_record_cci_dvfs_step(unsigned int step);
extern void aee_record_cpu_dvfs_cb(unsigned int step);
extern void aee_record_cpufreq_cb(unsigned int step);
extern void aee_record_cpu_volt(struct mt_cpu_dvfs *p, unsigned int volt);
extern void aee_record_freq_idx(struct mt_cpu_dvfs *p, int idx);

extern void _mt_cpufreq_aee_init(void);

#endif	/* __MTK_CPUFREQ_DEBUG_H__ */
