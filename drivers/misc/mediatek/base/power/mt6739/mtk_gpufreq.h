/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MT_GPUFREQ_H
#define _MT_GPUFREQ_H

#include <linux/module.h>
#include <linux/clk.h>

#define MAX_VCO_VALUE	3800000
#define MIN_VCO_VALUE	1500000

#define DIV2_MAX_FREQ   1900000
#define DIV2_MIN_FREQ   750000
#define DIV4_MAX_FREQ	950000
/* #define DIV4_MIN_FREQ	375000 */
#define DIV4_MIN_FREQ	250000
#define DIV8_MAX_FREQ   475000
#define DIV8_MIN_FREQ   187500
#define DIV16_MAX_FREQ  237500
#define DIV16_MIN_FREQ  93750

#define TO_MHz_HEAD 100
#define TO_MHz_TAIL 10
#define ROUNDING_VALUE 5
#define DDS_SHIFT 14
#define POST_DIV_SHIFT 24
#define POST_DIV_MASK 0x70000000
#define GPUPLL_FIN 26

#define BUCK_ON 1
#define BUCK_OFF 0
#define BUCK_ENFORCE_OFF 4

enum post_div_order_enum {
	POST_DIV2 = 1,
	POST_DIV4,
	POST_DIV8,
	POST_DIV16,
};

struct mt_gpufreq_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_vsram;
	unsigned int gpufreq_idx;
};

struct mt_gpufreq_power_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_volt;
	unsigned int gpufreq_power;
};

struct mt_gpufreq_clk_t {
	struct clk *clk_mux;          /* main clock for mfg setting */
	struct clk *clk_main_parent;	 /* substitution clock for mfg transient mux setting */
	struct clk *clk_sub_parent;	 /* substitution clock for mfg transient parent setting */
};

struct mt_gpufreq_pmic_t {
	struct regulator *reg_vproc;		/* vproc regulator */
	struct regulator *reg_vsram;	/* vproc sram regulator */
	struct regulator *reg_vcore;	/* vproc sram regulator */
};

/*****************
 * extern function
 ******************/
extern int mt_gpufreq_state_set(int enabled);
extern void mt_gpufreq_thermal_protect(unsigned int limited_power);
extern unsigned int mt_gpufreq_get_cur_freq_index(void);
extern unsigned int mt_gpufreq_get_cur_freq(void);
extern unsigned int mt_gpufreq_get_cur_volt(void);
extern unsigned int mt_gpufreq_get_dvfs_table_num(void);
extern unsigned int mt_gpufreq_target(unsigned int idx);
extern unsigned int mt_gpufreq_voltage_enable_set(unsigned int enable);
extern unsigned int mt_gpufreq_voltage_lpm_set(unsigned int enable_lpm);
extern unsigned int mt_gpufreq_update_volt(unsigned int pmic_volt[], unsigned int array_size);
extern unsigned int mt_gpufreq_get_freq_by_idx(unsigned int idx);
extern unsigned int mt_gpufreq_get_volt_by_idx(unsigned int idx);
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
extern void mt_gpufreq_set_loading(unsigned int gpu_loading);
extern int mt_gpufreq_get_cur_ceiling_idx(void);

extern unsigned int mt_get_mfgclk_freq(void);	/* Freq Meter API */
extern unsigned int mt_get_ckgen_freq(unsigned int);
extern u32 get_devinfo_with_index(u32 index);
extern int mt_gpufreq_fan53555_init(void);
#ifdef CONFIG_THERMAL
extern int mtk_gpufreq_register(struct mt_gpufreq_power_table_info *freqs, int num);
#endif
/* #ifdef MT_GPUFREQ_AEE_RR_REC */
extern void aee_rr_rec_gpu_dvfs_vgpu(u8 val);
extern void aee_rr_rec_gpu_dvfs_oppidx(u8 val);
extern void aee_rr_rec_gpu_dvfs_status(u8 val);
extern u8 aee_rr_curr_gpu_dvfs_status(void);
/* #endif */

/*****************
 * power limit notification
 ******************/
typedef void (*gpufreq_power_limit_notify)(unsigned int);
extern void mt_gpufreq_power_limit_notify_registerCB(gpufreq_power_limit_notify pCB);

/*****************
 * input boost notification
 ******************/
typedef void (*gpufreq_input_boost_notify)(unsigned int);
extern void mt_gpufreq_input_boost_notify_registerCB(gpufreq_input_boost_notify pCB);

/*****************
 * update voltage notification
 ******************/
typedef void (*gpufreq_ptpod_update_notify)(void);
extern void mt_gpufreq_update_volt_registerCB(gpufreq_ptpod_update_notify pCB);

/*****************
 * profiling purpose
 ******************/
typedef void (*sampler_func)(unsigned int);
extern void mt_gpufreq_setfreq_registerCB(sampler_func pCB);
extern void mt_gpufreq_setvolt_registerCB(sampler_func pCB);

extern void switch_mfg_clk(int src);


#ifdef MTK_GPU_SPM
void mtk_gpu_spm_fix_by_idx(unsigned int idx);
void mtk_gpu_spm_reset_fix(void);
void mtk_gpu_spm_pause(void);
void mtk_gpu_spm_resume(void);
#endif

#endif
