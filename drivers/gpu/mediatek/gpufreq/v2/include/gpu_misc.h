/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPU_MISC_H__
#define __GPU_MISC_H__

#include <linux/module.h>
/**************************************************
 * Platform Implementation
 **************************************************/
struct gpudfd_platform_fp {
	unsigned int (*get_dfd_force_dump_mode)(void);
	void (*set_dfd_force_dump_mode)(unsigned int mode);
	void (*config_dfd)(unsigned int enable);
};

typedef void (*gpufreq_power_limit_notify)(unsigned int); /* legacy */
/**************************************************
 * External Function
 **************************************************/
void gpufreq_hardstop_dump_slog(void);
void gpu_misc_register_gpudfd_fp(struct gpudfd_platform_fp *dfd_platform_fp);
unsigned int gpufreq_get_dfd_force_dump_mode(void);
unsigned int gpufreq_set_dfd_force_dump_mode(unsigned int mode);
void gpufreq_config_dfd(unsigned int enable);
/**************************************************
 * MTK GPUFREQ API
 **************************************************/
extern void mt_gpufreq_set_hwapm_state(bool bEnableHWAPM);
extern unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable);
extern unsigned int mt_gpufreq_get_thermal_limit_index(void);
extern unsigned int mt_gpufreq_get_thermal_limit_freq(void);
extern unsigned int mt_gpufreq_get_cur_freq_index(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern int mt_gpufreq_get_cur_ceiling_idx(void);
extern void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB);

/* PTPOD for legacy chip*/
extern unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size);
extern void mt_gpufreq_enable_by_ptpod(void);
extern void mt_gpufreq_disable_by_ptpod(void);
extern void mt_gpufreq_restore_default_volt(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx);

/* Thermal for legacy chip*/
extern struct mt_gpufreq_power_table_info *mt_gpufreq_get_power_table(void);
extern unsigned int mt_gpufreq_get_seg_max_opp_index(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern unsigned int mt_gpufreq_get_power_table_num(void);
extern void mt_gpufreq_set_gpu_wrap_fp(int (*gpu_wrap_fp)(void));

#endif /* __GPU_MISC_H__ */
