/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef ___MT_GPUFREQ_PLAT_H___
#define ___MT_GPUFREQ_PLAT_H___

#include <linux/module.h>
#include <linux/clk.h>

#define MT_GPUFREQ_BRINGUP                      0
#define MT_GPUFREQ_GED_READY                    0
#define MT_GPUFREQ_KICKER_PBM_READY             0
#define MT_GPUFREQ_STATIC_PWR_READY2USE         0
#define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT        0
#define MT_GPUFREQ_BATT_PERCENT_PROTECT         0
#define MT_GPUFREQ_BATT_OC_PROTECT              0
#define MT_GPUFREQ_DYNAMIC_POWER_TABLE_UPDATE   0

#define GPUFERQ_TAG	"[GPU/DVFS] "
#define gpufreq_pr_info(fmt, args...)	pr_info(GPUFERQ_TAG fmt, ##args)
#define gpufreq_pr_debug(fmt, args...)	pr_debug(GPUFERQ_TAG fmt, ##args)

#define gpufreq_pr_mask(log_mask, module, fmt, args...)		\
do {								\
	if (!log_mask)						\
		gpufreq_pr_debug(fmt, ##args);			\
	else if (log_mask & (1 << module))			\
		gpufreq_pr_info(fmt, ##args);			\
	else							\
		gpufreq_pr_debug(fmt, ##args);			\
} while (0)

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};

enum mt_gpufreq_kicker {
	KIR_PROC = 0,
	KIR_DDK,
	KIR_PTPOD,
	KIR_THERMAL,
};

enum mt_gpufreq_log_mask {
	M_DVFS = 0,
	M_BUCK,
	M_PTPOD,
	M_ALL = (1 << M_DVFS) | (1 << M_BUCK) | (1 << M_PTPOD),
};

enum mt_power_state {
	POWER_OFF = 0,
	POWER_ON,
};

enum mt_cg_state {
	CG_OFF = 0,
	CG_ON,
	CG_KEEP,
};

enum mt_mtcmos_state {
	MTCMOS_OFF = 0,
	MTCMOS_ON,
	MTCMOS_KEEP,
};

enum mt_buck_state {
	BUCK_OFF = 0,
	BUCK_ON,
	BUCK_KEEP,
};

/**
 * MTK GPUFREQ API
 */
extern unsigned int mt_gpufreq_bringup(void);
extern unsigned int mt_gpufreq_get_dvfs_en(void);
extern unsigned int mt_gpufreq_power_ctl_en(void);
extern unsigned int mt_gpufreq_get_cust_init_en(void);
extern unsigned int mt_gpufreq_get_cur_freq_index(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern unsigned int mt_gpufreq_get_real_dvfs_table_num(void);
extern unsigned int mt_gpufreq_target(unsigned int request_idx,
		enum mt_gpufreq_kicker);
extern unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[],
		unsigned int array_size);
extern unsigned int mt_gpufreq_get_freq_by_real_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_real_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_ori_opp_idx(unsigned int idx);
extern struct mt_gpufreq_power_table_info *pass_gpu_table_to_eara(void);
extern unsigned int mt_gpufreq_get_seg_max_opp_index(void);
extern void mt_gpufreq_thermal_protect(unsigned int limited_power);
extern void mt_gpufreq_restore_default_volt(void);
extern void mt_gpufreq_enable_by_ptpod(void);
extern void mt_gpufreq_disable_by_ptpod(void);
extern unsigned int mt_gpufreq_get_max_power(void);
extern unsigned int mt_gpufreq_get_min_power(void);
extern unsigned int mt_gpufreq_get_thermal_limit_index(void);
extern unsigned int mt_gpufreq_get_thermal_limit_freq(void);
extern void mt_gpufreq_set_power_limit_by_pbm(unsigned int limited_power);
extern unsigned int mt_gpufreq_get_leakage_mw(void);
extern int mt_gpufreq_get_cur_ceiling_idx(void);
extern void mt_gpufreq_set_loading(unsigned int gpu_loading); /* legacy */
extern void mt_gpufreq_power_control(enum mt_power_state, enum mt_cg_state,
		enum mt_mtcmos_state, enum mt_buck_state);
extern void mt_gpufreq_set_timestamp(void);
extern void mt_gpufreq_check_bus_idle(void);

#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_gpu_dvfs_vgpu(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_vgpu(void);
extern void aee_rr_rec_gpu_dvfs_oppidx(u8 val);
extern void aee_rr_rec_gpu_dvfs_status(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_status(void);
#endif

/**
 * power limit notification
 */
typedef void (*gpufreq_power_limit_notify)(unsigned int);
extern void mt_gpufreq_power_limit_notify_registerCB(
		gpufreq_power_limit_notify pCB);

/**
 * input boost notification
 */
typedef void (*gpufreq_input_boost_notify)(unsigned int);
extern void mt_gpufreq_input_boost_notify_registerCB(
		gpufreq_input_boost_notify pCB);

#endif /* ___MT_GPUFREQ_PLAT_H___ */
