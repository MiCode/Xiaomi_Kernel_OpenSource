/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#ifdef CONFIG_MACH_MT8168
#include "mtk_power_throttle.h"
#endif
#include "mt-plat/mtk_thermal_platform.h"
#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <tscpu_settings.h>
#include <mt-plat/aee.h>
#include <linux/uidgid.h>
#include <ap_thermal_limit.h>
#if (CONFIG_THERMAL_AEE_RR_REC == 1)
#include <mtk_ram_console.h>
#endif
#ifdef ATM_USES_PPM
#include "mtk_ppm_api.h"
#include "mtk_ppm_platform.h"
#else
#ifndef CONFIG_MACH_MT8168
#include "mt_cpufreq.h"
#endif
#endif

#ifdef FAST_RESPONSE_ATM
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#endif
#include "clatm_initcfg.h"
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include "mtk_thermal_ipi.h"
#include "linux/delay.h"
#endif
#if defined(THERMAL_VPU_SUPPORT)
#if defined(CONFIG_MTK_APUSYS_SUPPORT)
#include "apu_power_table.h"
#else
#include "vpu_dvfs.h"
#endif
#endif
#if defined(THERMAL_MDLA_SUPPORT)
#if defined(CONFIG_MTK_APUSYS_SUPPORT)
#include "apu_power_table.h"
#else
#include "mdla_dvfs.h"
#endif
#endif
#if defined(CATM_TPCB_EXTEND)
#include <mt-plat/mtk_devinfo.h>
#endif

/*****************************************************************************
 *  Local switches
 *****************************************************************************/
/* Define ATM_CFG_PROFILING to compile the code segments for profiling. It
 * depends on ostimer which is wrapped by CFG_TIMER_SUPPORT.
 * Only tested with FAST_RESPONSE_ATM is turned on.
 * ! Must not define this in official release branches. Only for local tests.
 */
/* #define ATM_CFG_PROFILING (1) */

/*=============================================================
 *Local variable definition
 *=============================================================
 */
static int print_cunt;
static int adaptive_limit[5][2];
static struct apthermolmt_user ap_atm;
static char *ap_atm_log = "ap_atm";

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
unsigned int adaptive_cpu_power_limit = 0x7FFFFFFF;
unsigned int adaptive_gpu_power_limit = 0x7FFFFFFF;
#if defined(THERMAL_VPU_SUPPORT)
unsigned int adaptive_vpu_power_limit = 0x7FFFFFFF;
#endif
#if defined(THERMAL_MDLA_SUPPORT)
unsigned int adaptive_mdla_power_limit = 0x7FFFFFFF;
#endif
static unsigned int prv_adp_cpu_pwr_lim;
static unsigned int prv_adp_gpu_pwr_lim;
#if defined(THERMAL_VPU_SUPPORT)
static unsigned int prv_adp_vpu_pwr_lim;
#endif
#if defined(THERMAL_MDLA_SUPPORT)
static unsigned int prv_adp_mdla_pwr_lim;
#endif
unsigned int gv_cpu_power_limit = 0x7FFFFFFF;
unsigned int gv_gpu_power_limit = 0x7FFFFFFF;
#if CPT_ADAPTIVE_AP_COOLER
static int TARGET_TJ = 65000;
static int cpu_target_tj = 65000;
static int cpu_target_offset = 10000;
static int TARGET_TJ_HIGH = 66000;
static int TARGET_TJ_LOW = 64000;
static int PACKAGE_THETA_JA_RISE = 10;
static int PACKAGE_THETA_JA_FALL = 10;
static int MINIMUM_CPU_POWER = 500;
static int MAXIMUM_CPU_POWER = 1240;
static int MINIMUM_GPU_POWER = 676;
static int MAXIMUM_GPU_POWER = 676;
static int MINIMUM_TOTAL_POWER = 500 + 676;
static int MAXIMUM_TOTAL_POWER = 1240 + 676;
static int FIRST_STEP_TOTAL_POWER_BUDGET = 1750;
#if defined(THERMAL_VPU_SUPPORT)
static int MINIMUM_VPU_POWER = 300;
static int MAXIMUM_VPU_POWER = 1000;
#endif
#if defined(THERMAL_MDLA_SUPPORT)
static int MINIMUM_MDLA_POWER = 300;
static int MAXIMUM_MDLA_POWER = 1000;
#endif

/* 1. MINIMUM_BUDGET_CHANGE = 0 ==> thermal equilibrium
 * maybe at higher than TARGET_TJ_HIGH
 */
/* 2. Set MINIMUM_BUDGET_CHANGE > 0 if to keep Tj at TARGET_TJ */
static int MINIMUM_BUDGET_CHANGE = 50;
static int g_total_power;
#if defined(THERMAL_VPU_SUPPORT) || defined(THERMAL_MDLA_SUPPORT)
static int g_delta_power;
#endif
#endif

#if CPT_ADAPTIVE_AP_COOLER
static struct thermal_cooling_device
	*cl_dev_adp_cpu[MAX_CPT_ADAPTIVE_COOLERS] = { NULL };
static unsigned int cl_dev_adp_cpu_state[MAX_CPT_ADAPTIVE_COOLERS] = { 0 };
#if defined(CLATM_SET_INIT_CFG)
int TARGET_TJS[MAX_CPT_ADAPTIVE_COOLERS] = {
			CLATM_INIT_CFG_0_TARGET_TJ,
			CLATM_INIT_CFG_1_TARGET_TJ,
			CLATM_INIT_CFG_2_TARGET_TJ };
#else
int TARGET_TJS[MAX_CPT_ADAPTIVE_COOLERS] = { 85000, 0 };
#endif

static unsigned int cl_dev_adp_cpu_state_active;
#endif	/* end of CPT_ADAPTIVE_AP_COOLER */

#if CPT_ADAPTIVE_AP_COOLER
char *adaptive_cooler_name = "cpu_adaptive_";

#if defined(CLATM_SET_INIT_CFG)
static int FIRST_STEP_TOTAL_POWER_BUDGETS[MAX_CPT_ADAPTIVE_COOLERS] = {
					CLATM_INIT_CFG_0_FIRST_STEP,
					CLATM_INIT_CFG_1_FIRST_STEP,
					CLATM_INIT_CFG_2_FIRST_STEP };

static int PACKAGE_THETA_JA_RISES[MAX_CPT_ADAPTIVE_COOLERS] = {
					CLATM_INIT_CFG_0_THETA_RISE,
					CLATM_INIT_CFG_1_THETA_RISE,
					CLATM_INIT_CFG_2_THETA_RISE };

static int PACKAGE_THETA_JA_FALLS[MAX_CPT_ADAPTIVE_COOLERS] = {
					CLATM_INIT_CFG_0_THETA_FALL,
					CLATM_INIT_CFG_1_THETA_FALL,
					CLATM_INIT_CFG_2_THETA_FALL };

static int MINIMUM_BUDGET_CHANGES[MAX_CPT_ADAPTIVE_COOLERS] = {
					CLATM_INIT_CFG_0_MIN_BUDGET_CHG,
					CLATM_INIT_CFG_1_MIN_BUDGET_CHG,
					CLATM_INIT_CFG_2_MIN_BUDGET_CHG };

static int MINIMUM_CPU_POWERS[MAX_CPT_ADAPTIVE_COOLERS] = {
					CLATM_INIT_CFG_0_MIN_CPU_PWR,
					CLATM_INIT_CFG_1_MIN_CPU_PWR,
					CLATM_INIT_CFG_2_MIN_CPU_PWR };

static int MAXIMUM_CPU_POWERS[MAX_CPT_ADAPTIVE_COOLERS] = {
					CLATM_INIT_CFG_0_MAX_CPU_PWR,
					CLATM_INIT_CFG_1_MAX_CPU_PWR,
					CLATM_INIT_CFG_2_MAX_CPU_PWR };

static int MINIMUM_GPU_POWERS[MAX_CPT_ADAPTIVE_COOLERS] = {
					CLATM_INIT_CFG_0_MIN_GPU_PWR,
					CLATM_INIT_CFG_1_MIN_GPU_PWR,
					CLATM_INIT_CFG_2_MIN_GPU_PWR };

static int MAXIMUM_GPU_POWERS[MAX_CPT_ADAPTIVE_COOLERS] = {
					CLATM_INIT_CFG_0_MAX_GPU_PWR,
					CLATM_INIT_CFG_1_MAX_GPU_PWR,
					CLATM_INIT_CFG_2_MAX_GPU_PWR };
#else
static int FIRST_STEP_TOTAL_POWER_BUDGETS[MAX_CPT_ADAPTIVE_COOLERS] = {
								3300, 0 };

static int PACKAGE_THETA_JA_RISES[MAX_CPT_ADAPTIVE_COOLERS] = { 35, 0 };
static int PACKAGE_THETA_JA_FALLS[MAX_CPT_ADAPTIVE_COOLERS] = { 25, 0 };
static int MINIMUM_BUDGET_CHANGES[MAX_CPT_ADAPTIVE_COOLERS] = { 50, 0 };
static int MINIMUM_CPU_POWERS[MAX_CPT_ADAPTIVE_COOLERS] = { 1200, 0 };
static int MAXIMUM_CPU_POWERS[MAX_CPT_ADAPTIVE_COOLERS] = { 4400, 0 };
static int MINIMUM_GPU_POWERS[MAX_CPT_ADAPTIVE_COOLERS] = { 350, 0 };
static int MAXIMUM_GPU_POWERS[MAX_CPT_ADAPTIVE_COOLERS] = { 960, 0 };
#endif
static int is_max_gpu_power_specified[MAX_CPT_ADAPTIVE_COOLERS] = { 0, 0 };

#ifndef CLATM_USE_MIN_CPU_OPP
#define CLATM_USE_MIN_CPU_OPP			(0)
#endif

#if CLATM_USE_MIN_CPU_OPP
struct atm_cpu_min_opp {
	/* mode is
	 * 0: Didn't initialize or Some errors occurred
	 * 1: Use a min CPU power budget to guarantee minimum performance
	 * 2: Use a set of CPU OPPs to gurantee minimum performance
	 */
	int mode[MAX_CPT_ADAPTIVE_COOLERS];
	/* To keep original min CPU power budgets */
	int min_CPU_power[MAX_CPT_ADAPTIVE_COOLERS];
	/* To keep min CPU power budgets calculated from a set of CPU OPP */
	int min_CPU_power_from_opp[MAX_CPT_ADAPTIVE_COOLERS];
	struct ppm_cluster_status
		cpu_opp_set[MAX_CPT_ADAPTIVE_COOLERS][NR_PPM_CLUSTERS];
};
static struct atm_cpu_min_opp g_c_min_opp;
#endif

#if defined(CLATM_SET_INIT_CFG)
static int active_adp_cooler = CLATM_INIT_CFG_ACTIVE_ATM_COOLER;
#else
static int active_adp_cooler;
#endif

static int GPU_L_H_TRIP = 80, GPU_L_L_TRIP = 40;

/* tscpu_atm
 *   0: ATMv1 (default)
 *   1: ATMv2 (FTL)
 *   2: CPU_GPU_Weight ATM v2
 *   3: Precise Power Budgeting + Hybrid Power Budgeting
 */
#ifdef PHPB_DEFAULT_ON
static int tscpu_atm = 3;
#else
static int tscpu_atm = 1;
#endif
static int tt_ratio_high_rise = 1;
static int tt_ratio_high_fall = 1;
static int tt_ratio_low_rise = 1;
static int tt_ratio_low_fall = 1;
static int tp_ratio_high_rise = 1;
static int tp_ratio_high_fall;
static int tp_ratio_low_rise;
static int tp_ratio_low_fall;
/* static int cpu_loading = 0; */

static int (*_adaptive_power_calc)
(long prev_temp, long curr_temp, unsigned int gpu_loading);

#if PRECISE_HYBRID_POWER_BUDGET
/* initial value: assume 1 degreeC for temp.
 *			<=> 1 unit for total_power(0~100)
 */
struct phpb_param {
	int tt, tp;
	char type[8];
};

enum {
	PHPB_PARAM_CPU = 0,
	PHPB_PARAM_GPU,
	NR_PHPB_PARAMS
};

static struct phpb_param phpb_params[NR_PHPB_PARAMS];
static const int phpb_theta_min = 1;
static int phpb_theta_max = 4;
static int tj_stable_range = 1000;

#if 0
#define MAX_GPU_POWER_SMA_LEN	(32)
static unsigned int gpu_power_sma_len = 1;
static unsigned int gpu_power_history[MAX_GPU_POWER_SMA_LEN];
static unsigned int gpu_power_history_idx;
#endif
#endif

#if THERMAL_HEADROOM
static int p_Tpcb_correlation;
static int Tpcb_trip_point;
static int thp_max_cpu_power;
static int thp_p_tj_correlation;
static int thp_threshold_tj;
#endif

#if CONTINUOUS_TM
#if defined(CLATM_SET_INIT_CFG)
static int ctm_on = CLATM_INIT_CFG_CATM; /* 2: cATM+, 1: cATMv1, 0: off */
#else
static int ctm_on = -1; /* 2: cATM+, 1: cATMv1, 0: off */
#endif
static int MAX_TARGET_TJ = -1;
static int STEADY_TARGET_TJ = -1;
static int TRIP_TPCB = -1;
static int STEADY_TARGET_TPCB = -1;
static int MAX_EXIT_TJ = -1;
static int STEADY_EXIT_TJ = -1;
static int COEF_AE = -1;
static int COEF_BE = -1;
static int COEF_AX = -1;
static int COEF_BX = -1;
#if defined(CATM_TPCB_EXTEND)
static int TPCB_EXTEND = -1;
static int g_turbo_bin;
#endif

/* static int current_TTJ = -1; */
static int current_ETJ = -1;

/* +++ cATM+ parameters +++ */
/* slope of base_ttj, automatically calculated */
static int K_TT = 4000;
#define MAX_K_SUM_TT	(K_TT * 10)
/* for ATM polling delay 50ms, increase this based on polling delay */
static int K_SUM_TT_LOW = 10;
/* for ATM polling delay 50ms, increase this based on polling delay */
static int K_SUM_TT_HIGH = 10;
/* clamp sum_tt (err_integral) between MIN_SUM_TT ~ MAX_SUM_TT,
 *	automatically calculated
 */
static int MIN_SUM_TT = -800000;
static int MAX_SUM_TT = 800000;
static int MIN_TTJ = 65000;

/* magic number decided by experience */
static int CATMP_STEADY_TTJ_DELTA = 10000;
/* --- cATM+ parameters --- */
#endif
#endif	/* end of CPT_ADAPTIVE_AP_COOLER */

#ifdef FAST_RESPONSE_ATM
#define KRTATM_HR	(1)
#define KRTATM_NORMAL	(2)
#define KRTATM_TIMER	KRTATM_NORMAL

#define TS_MS_TO_NS(x) (x * 1000 * 1000)
#if KRTATM_TIMER == KRTATM_HR
static struct hrtimer atm_hrtimer;
static unsigned long atm_hrtimer_polling_delay =
				TS_MS_TO_NS(CLATM_INIT_HRTIMER_POLLING_DELAY);

#elif KRTATM_TIMER == KRTATM_NORMAL
	static struct timer_list atm_timer;
	static unsigned long atm_timer_polling_delay =
		CLATM_INIT_HRTIMER_POLLING_DELAY;
	/**
	 * If curr_temp >= polling_trip_temp0, use interval/polling_factor0
	 * else If curr_temp >= polling_trip_temp1, use interval
	 * else if cur_temp >= polling_trip_temp2 &&
	 * curr_temp < polling_trip_temp1,
	 * use interval*polling_factor1
	 * else, use interval*polling_factor2
	 */
	static int polling_trip_temp0 = 75000;
	static int polling_trip_temp1 = 65000;
	static int polling_trip_temp2 = 40000;
	static int polling_factor0 = 10;
	static int polling_factor1 = 2;
	static int polling_factor2 = 4;
#endif
static int atm_curr_maxtj;
static int atm_prev_maxtj;
static int krtatm_curr_maxtj;
static int krtatm_prev_maxtj;
static u64 atm_curr_maxtj_time;
static u64 atm_prev_maxtj_time;
static struct task_struct *krtatm_thread_handle;
#endif

#ifdef ATM_CFG_PROFILING
int atm_resumed = 1;

s64 atm_period_min_delay = 0xFFFFFFF;
s64 atm_period_max_delay;
s64 atm_period_avg_delay;
unsigned int atm_period_cnt = 1;

s64 atm_exec_min_delay = 0xFFFFFFF;
s64 atm_exec_max_delay;
s64 atm_exec_avg_delay;
unsigned int atm_exec_cnt = 1;

s64 cpu_pwr_lmt_min_delay = 0xFFFFFFF;
s64 cpu_pwr_lmt_max_delay;
s64 cpu_pwr_lmt_avg_delay;
s64 cpu_pwr_lmt_latest_delay;
unsigned int cpu_pwr_lmt_cnt = 1;

s64 gpu_pwr_lmt_min_delay = 0xFFFFFFF;
s64 gpu_pwr_lmt_max_delay;
s64 gpu_pwr_lmt_avg_delay;
s64 gpu_pwr_lmt_latest_delay;
unsigned int gpu_pwr_lmt_cnt = 1;
#endif


#if defined(THERMAL_APU_UNLIMIT)
static unsigned long total_apu_polling_time;
#endif
/*=============================================================
 *Local function prototype
 *=============================================================
 */
static void set_adaptive_gpu_power_limit(unsigned int limit);
/*=============================================================
 *Weak functions
 *=============================================================
 */
#if 0
#ifdef ATM_USES_PPM
void __attribute__ ((weak))
mt_ppm_cpu_thermal_protect(unsigned int limited_power)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}
#else
void __attribute__ ((weak))
mt_cpufreq_thermal_protect(unsigned int limited_power)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}
#endif
#endif

int __attribute__((weak))
mtk_eara_thermal_pb_handle(int total_pwr_budget,
			   int max_cpu_power, int max_gpu_power,
			   int max_vpu_power,  int max_mdla_power)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}

bool __attribute__((weak))
mtk_get_gpu_loading(unsigned int *pLoading)
{
#ifdef CONFIG_MTK_GPU_SUPPORT
	pr_notice("E_WF: %s doesn't exist\n", __func__);
#endif
	return 0;
}
unsigned int  __attribute__((weak))
mt_gpufreq_get_min_power(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}
unsigned int  __attribute__((weak))
mt_gpufreq_get_max_power(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}
void __attribute__ ((weak))
print_risky_temps(char *prefix, int offset, int printLevel)
{

}

unsigned int __attribute__ ((weak))
mt_gpufreq_get_cur_freq(void)
{
	return 0;
}

unsigned int __attribute__ ((weak))
mt_ppm_thermal_get_cur_power(void)
{
	return 0;
}

unsigned int __attribute__ ((weak))
mt_ppm_thermal_get_min_power(void)
{
	return 0;
}


void  __attribute__ ((weak))
set_uartlog_status(bool value)
{
}

bool  __attribute__ ((weak))
mt_get_uartlog_status(void)
{
	return 0;
}

/*=============================================================*/

/*
 *   static int step0_mask[11] = {1,1,1,1,1,1,1,1,1,1,1};
 *   static int step1_mask[11] = {0,1,1,1,1,1,1,1,1,1,1};
 *   static int step2_mask[11] = {0,0,1,1,1,1,1,1,1,1,1};
 *   static int step3_mask[11] = {0,0,0,1,1,1,1,1,1,1,1};
 *   static int step4_mask[11] = {0,0,0,0,1,1,1,1,1,1,1};
 *   static int step5_mask[11] = {0,0,0,0,0,1,1,1,1,1,1};
 *  static int step6_mask[11] = {0,0,0,0,0,0,1,1,1,1,1};
 *   static int step7_mask[11] = {0,0,0,0,0,0,0,1,1,1,1};
 *   static int step8_mask[11] = {0,0,0,0,0,0,0,0,1,1,1};
 *   static int step9_mask[11] = {0,0,0,0,0,0,0,0,0,1,1};
 *   static int step10_mask[11]= {0,0,0,0,0,0,0,0,0,0,1};
 */

int tsatm_thermal_get_catm_type(void)
{
	tscpu_dprintk("%s ctm_on = %d\n", __func__, ctm_on);
	return ctm_on;
}

int mtk_thermal_get_tpcb_target(void)
{
	return STEADY_TARGET_TPCB;
}
EXPORT_SYMBOL(mtk_thermal_get_tpcb_target);

/**
 * TODO: What's the diff from get_cpu_target_tj?
 */
int get_target_tj(void)
{
	return TARGET_TJ;
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM &&	\
	CPT_ADAPTIVE_AP_COOLER && PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
/* ATM in SSPM requires ATM, PPB, CATM */
static int atm_sspm_enabled;
static int atm_prev_active_atm_cl_id = -100;

static DEFINE_MUTEX(atm_cpu_lmt_mutex);
static DEFINE_MUTEX(atm_gpu_lmt_mutex);
#if defined(THERMAL_VPU_SUPPORT)
static DEFINE_MUTEX(atm_vpu_lmt_mutex);
#endif
#if defined(THERMAL_MDLA_SUPPORT)
static DEFINE_MUTEX(atm_mdla_lmt_mutex);
#endif

static int atm_update_atm_param_to_sspm(void)
{
	int ret = 0;
	struct thermal_ipi_data thermal_data;
	int ackData = 0;

	/* ATM, modified in decide_ttj(). */
	thermal_data.u.data.arg[0] = MINIMUM_CPU_POWER;
	thermal_data.u.data.arg[1] = MAXIMUM_CPU_POWER;
	while ((ret = atm_to_sspm(THERMAL_IPI_SET_ATM_CFG_GRP1, 2,
			&thermal_data, &ackData)) && ret < 0)
		mdelay(1);

	/* ATM, modified in decide_ttj(). */
	thermal_data.u.data.arg[0] = MINIMUM_GPU_POWER;
	thermal_data.u.data.arg[1] = MAXIMUM_GPU_POWER;

	/* modified in mtk_ts_cpu*::tscpu_write(). */
	thermal_data.u.data.arg[2] = TARGET_TJS[0];
	while ((ret = atm_to_sspm(THERMAL_IPI_SET_ATM_CFG_GRP2, 3,
			&thermal_data, &ackData)) && ret < 0)
		mdelay(1);

	return ackData;
}

static int atm_update_ppb_param_to_sspm(void)
{
	int ret = 0;
	struct thermal_ipi_data thermal_data;
	int ackData = 0;

	/* PPB, modified in tscpu_write_phpb(), and phpb_params_init(). */
	thermal_data.u.data.arg[0] = phpb_params[PHPB_PARAM_CPU].tt;
	thermal_data.u.data.arg[1] = phpb_params[PHPB_PARAM_CPU].tp;
	while ((ret = atm_to_sspm(THERMAL_IPI_SET_ATM_CFG_GRP3, 2,
			&thermal_data, &ackData)) && ret < 0)
		mdelay(1);

	/* PPB, modified in tscpu_write_phpb(), and phpb_params_init(). */
	thermal_data.u.data.arg[0] = phpb_params[PHPB_PARAM_GPU].tt;
	thermal_data.u.data.arg[1] = phpb_params[PHPB_PARAM_GPU].tp;
	while ((ret = atm_to_sspm(THERMAL_IPI_SET_ATM_CFG_GRP4, 2,
			&thermal_data, &ackData)) && ret < 0)
		mdelay(1);

	/* PPB, modified in tscpu_write_phpb(), or not modified at all. */
	thermal_data.u.data.arg[0] = tj_stable_range; /* Not modified. */
	thermal_data.u.data.arg[1] = phpb_theta_min; /* Not modified. */
	thermal_data.u.data.arg[2] = phpb_theta_max;
	while ((ret = atm_to_sspm(THERMAL_IPI_SET_ATM_CFG_GRP5, 3,
			&thermal_data, &ackData)) && ret < 0)
		mdelay(1);

	return ackData;
}

static int atm_update_catm_param_to_sspm(void)
{
	int ret = 0;
	struct thermal_ipi_data thermal_data;
	int ackData = 0;

	/* CATM, modified in mtk_cooler_atm::tscpu_write_ctm(). */
	thermal_data.u.data.arg[0] = MAX_TARGET_TJ;
	while ((ret = atm_to_sspm(THERMAL_IPI_SET_ATM_CFG_GRP6, 1,
			&thermal_data, &ackData)) && ret < 0)
		mdelay(1);

	return ackData;
}

static int atm_update_cg_alloc_param_to_sspm(void)
{
	int ret = 0;
	struct thermal_ipi_data thermal_data;
	int ackData = 0;

	/* CG Allocation,
	 * modified in mtk_cooler_atm::tscpu_write_gpu_threshold().
	 */
	thermal_data.u.data.arg[0] = GPU_L_H_TRIP;
	thermal_data.u.data.arg[1] = GPU_L_L_TRIP;
	while ((ret = atm_to_sspm(THERMAL_IPI_SET_ATM_CFG_GRP7, 2,
			&thermal_data, &ackData)) && ret < 0)
		mdelay(1);

	return ackData;
}

static int atm_update_ttj_to_sspm(void)
{
	int ret = 0;
	struct thermal_ipi_data thermal_data;
	int ackData = 0;

	thermal_data.u.data.arg[0] = TARGET_TJ;

	ret = atm_to_sspm(THERMAL_IPI_SET_ATM_TTJ, 1, &thermal_data, &ackData);
	if (ret < 0) {
		/* Retry one time. */
		mdelay(1);
		ret = atm_to_sspm(THERMAL_IPI_SET_ATM_TTJ, 1,
						&thermal_data, &ackData);
		if (ret < 0)
			tscpu_printk("%s ret %d ack %d\n", __func__,
								ret, ackData);
	}

	return ackData;
}

static int atm_enable_atm_in_sspm(int enable)
{
	int ret = 0;
	struct thermal_ipi_data thermal_data;
	int ackData = 0;

	if (enable == 0 || enable == 1) {
		thermal_data.u.data.arg[0] = enable;
		ret = atm_to_sspm(THERMAL_IPI_SET_ATM_EN, 1,
						&thermal_data, &ackData);
		if (ret < 0) {
			/* Retry one time. */
			mdelay(1);
			ret = atm_to_sspm(THERMAL_IPI_SET_ATM_EN, 1,
						&thermal_data, &ackData);
			if (ret < 0)
				tscpu_printk("%s ret %d ack %d\n", __func__,
								ret, ackData);
		}
	}

	return ackData;
}
#endif
#endif

#ifdef ATM_CFG_PROFILING
static void atm_profile_atm_period(s64 latest_latency)
{
	atm_period_max_delay = MAX(latest_latency, atm_period_max_delay);
	atm_period_min_delay = MIN(latest_latency, atm_period_min_delay);
	/* for 20ms ATM polling period,
	 * rolling avg of 2^14 roughly catchs 5min of data.
	 * 50 (Hz) * 60 (s) * 5 (min) = 15000.
	 */
	if (atm_period_cnt < 16384) {
		atm_period_avg_delay =
			(latest_latency + atm_period_avg_delay *
			(atm_period_cnt-1))/atm_period_cnt;

		atm_period_cnt++;
	} else {
		atm_period_avg_delay =
			(latest_latency + (atm_period_avg_delay<<14) -
						atm_period_avg_delay)>>14;
	}
	tscpu_warn("atm period M %lld m %lld a %lld l %lld\n"
		, atm_period_max_delay
		, atm_period_min_delay
		, atm_period_avg_delay
		, latest_latency);
}

static void atm_profile_atm_exec(s64 latest_latency)
{
	atm_exec_max_delay = MAX(latest_latency, atm_exec_max_delay);
	atm_exec_min_delay = MIN(latest_latency, atm_exec_min_delay);
	if (atm_exec_cnt < 16384) {
		atm_exec_avg_delay =
			(latest_latency + atm_exec_avg_delay *
			(atm_exec_cnt-1))/atm_exec_cnt;

		atm_exec_cnt++;
	} else {
		atm_exec_avg_delay =
			(latest_latency + (atm_exec_avg_delay<<14) -
						atm_exec_avg_delay)>>14;
	}
	tscpu_warn("atm exec delay M %lld m %lld a %lld l %lld\n"
		, atm_exec_max_delay
		, atm_exec_min_delay
		, atm_exec_avg_delay
		, latest_latency);
}

static void atm_profile_cpu_power_limit(s64 latest_latency)
{
	cpu_pwr_lmt_max_delay = MAX(latest_latency, cpu_pwr_lmt_max_delay);
	cpu_pwr_lmt_min_delay = MIN(latest_latency, cpu_pwr_lmt_min_delay);
	if (cpu_pwr_lmt_cnt < 16384) {
		cpu_pwr_lmt_avg_delay =
			(latest_latency + cpu_pwr_lmt_avg_delay *
			(cpu_pwr_lmt_cnt-1))/cpu_pwr_lmt_cnt;

		cpu_pwr_lmt_cnt++;
	} else {
		cpu_pwr_lmt_avg_delay =
			(latest_latency + (cpu_pwr_lmt_avg_delay<<14) -
						cpu_pwr_lmt_avg_delay)>>14;
	}
	tscpu_warn("cpu lmt delay M %lld m %lld a %lld l %lld\n"
		, cpu_pwr_lmt_max_delay
		, cpu_pwr_lmt_min_delay
		, cpu_pwr_lmt_avg_delay
		, latest_latency);
}

static void atm_profile_gpu_power_limit(s64 latest_latency)
{
	gpu_pwr_lmt_max_delay = MAX(latest_latency, gpu_pwr_lmt_max_delay);
	gpu_pwr_lmt_min_delay = MIN(latest_latency, gpu_pwr_lmt_min_delay);
	if (gpu_pwr_lmt_cnt < 16384) {
		gpu_pwr_lmt_avg_delay =
			(latest_latency + gpu_pwr_lmt_avg_delay *
			(gpu_pwr_lmt_cnt-1))/gpu_pwr_lmt_cnt;

		gpu_pwr_lmt_cnt++;
	} else {
		gpu_pwr_lmt_avg_delay =
			(latest_latency + (gpu_pwr_lmt_avg_delay<<14) -
						gpu_pwr_lmt_avg_delay)/1024;
	}
	tscpu_warn("gpu lmt delay M %lld m %lld a %lld l %lld\n"
		, gpu_pwr_lmt_max_delay
		, gpu_pwr_lmt_min_delay
		, gpu_pwr_lmt_avg_delay
		, latest_latency);
}
#endif

static void set_adaptive_cpu_power_limit(unsigned int limit)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	mutex_lock(&atm_cpu_lmt_mutex);
#endif
#endif

	prv_adp_cpu_pwr_lim = adaptive_cpu_power_limit;
	adaptive_cpu_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	if (atm_sspm_enabled)
		adaptive_cpu_power_limit = 0x7FFFFFFF;
#endif
#endif

	if (prv_adp_cpu_pwr_lim != adaptive_cpu_power_limit) {
#ifdef ATM_CFG_PROFILING
		ktime_t now, delta;
#endif

		/* print debug log */
		adaptive_limit[print_cunt][0] =
			(int) (adaptive_cpu_power_limit != 0x7FFFFFFF) ?
						adaptive_cpu_power_limit : 0;

#ifdef FAST_RESPONSE_ATM
		adaptive_limit[print_cunt][1] = krtatm_curr_maxtj;
#else
		adaptive_limit[print_cunt][1] = tscpu_get_curr_temp();
#endif
		print_cunt++;
		if (print_cunt == 5) {
			tscpu_warn(
				"%s (0x%x) %d T=%d, %d T=%d, %d T=%d, %d T=%d, %d T=%d\n",
				__func__,
				tscpu_get_temperature_range(),
				adaptive_limit[4][0], adaptive_limit[4][1],
				adaptive_limit[3][0], adaptive_limit[3][1],
				adaptive_limit[2][0], adaptive_limit[2][1],
				adaptive_limit[1][0], adaptive_limit[1][1],
				adaptive_limit[0][0], adaptive_limit[0][1]);

			print_cunt = 0;
		} else {
#ifdef FAST_RESPONSE_ATM
			if ((prv_adp_cpu_pwr_lim != 0x7FFFFFFF) &&
				((adaptive_cpu_power_limit + 1000)
				< prv_adp_cpu_pwr_lim))
				tscpu_warn(
					"%s Big delta power %u curr_T=%d, %u prev_T=%d\n",
					__func__, adaptive_cpu_power_limit,
					krtatm_curr_maxtj, prv_adp_cpu_pwr_lim,
					krtatm_prev_maxtj);
#endif
		}

#ifdef ATM_CFG_PROFILING
		now = ktime_get();
#endif
		apthermolmt_set_cpu_power_limit(&ap_atm,
					adaptive_cpu_power_limit);

#ifdef ATM_CFG_PROFILING
		delta = ktime_get();
		if (ktime_after(delta, now)) {
			cpu_pwr_lmt_latest_delay =
					ktime_to_us(ktime_sub(delta, now));

			atm_profile_cpu_power_limit(cpu_pwr_lmt_latest_delay);
		}
#endif
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	mutex_unlock(&atm_cpu_lmt_mutex);
#endif
#endif
}

static void set_adaptive_gpu_power_limit(unsigned int limit)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
		mutex_lock(&atm_gpu_lmt_mutex);
#endif
#endif

	prv_adp_gpu_pwr_lim = adaptive_gpu_power_limit;
	adaptive_gpu_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	if (atm_sspm_enabled)
		adaptive_gpu_power_limit = 0x7FFFFFFF;
#endif
#endif

	if (prv_adp_gpu_pwr_lim != adaptive_gpu_power_limit) {
#ifdef ATM_CFG_PROFILING
		ktime_t now, delta;
#endif

		tscpu_dprintk("%s %d\n", __func__,
				(adaptive_gpu_power_limit != 0x7FFFFFFF) ?
						adaptive_gpu_power_limit : 0);
#ifdef ATM_CFG_PROFILING
		now = ktime_get();
#endif
		apthermolmt_set_gpu_power_limit(&ap_atm,
						adaptive_gpu_power_limit);
#ifdef ATM_CFG_PROFILING
		delta = ktime_get();
		if (ktime_after(delta, now)) {
			gpu_pwr_lmt_latest_delay =
					ktime_to_us(ktime_sub(delta, now));

			atm_profile_gpu_power_limit(gpu_pwr_lmt_latest_delay);
		}
#endif

	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	mutex_unlock(&atm_gpu_lmt_mutex);
#endif
#endif
}

#if defined(THERMAL_VPU_SUPPORT)
static void set_adaptive_vpu_power_limit(unsigned int limit)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER && \
		PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
		mutex_lock(&atm_vpu_lmt_mutex);
#endif
#endif

	prv_adp_vpu_pwr_lim = adaptive_vpu_power_limit;
	adaptive_vpu_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER && \
		PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	if (atm_sspm_enabled)
		adaptive_vpu_power_limit = 0x7FFFFFFF;
#endif
#endif

	if (prv_adp_vpu_pwr_lim != adaptive_vpu_power_limit) {
		tscpu_dprintk("%s %d\n", __func__,
			(adaptive_vpu_power_limit != 0x7FFFFFFF) ?
			adaptive_vpu_power_limit : 0);
		apthermolmt_set_vpu_power_limit(&ap_atm,
			adaptive_vpu_power_limit);
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER && \
		PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	mutex_unlock(&atm_vpu_lmt_mutex);
#endif
#endif
}
#endif

#if defined(THERMAL_MDLA_SUPPORT)
static void set_adaptive_mdla_power_limit(unsigned int limit)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
		mutex_lock(&atm_mdla_lmt_mutex);
#endif
#endif

	prv_adp_mdla_pwr_lim = adaptive_mdla_power_limit;
	adaptive_mdla_power_limit = (limit != 0) ? limit : 0x7FFFFFFF;
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	if (atm_sspm_enabled)
		adaptive_mdla_power_limit = 0x7FFFFFFF;
#endif
#endif

	if (prv_adp_mdla_pwr_lim != adaptive_mdla_power_limit) {
		tscpu_dprintk("%s %d\n", __func__,
		     (adaptive_mdla_power_limit != 0x7FFFFFFF) ?
						adaptive_mdla_power_limit : 0);
		apthermolmt_set_mdla_power_limit(&ap_atm,
			adaptive_mdla_power_limit);
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	mutex_unlock(&atm_mdla_lmt_mutex);
#endif
#endif
}
#endif

#if CPT_ADAPTIVE_AP_COOLER
int is_cpu_power_unlimit(void)
{
	return (g_total_power == 0
			|| g_total_power >= MAXIMUM_TOTAL_POWER) ? 1 : 0;

}
EXPORT_SYMBOL(is_cpu_power_unlimit);

int is_cpu_power_min(void)
{
	return (g_total_power <= MINIMUM_TOTAL_POWER) ? 1 : 0;
}
EXPORT_SYMBOL(is_cpu_power_min);

/**
 * TODO: What's the diff from get_target_tj?
 */
int get_cpu_target_tj(void)
{
	return cpu_target_tj;
}
EXPORT_SYMBOL(get_cpu_target_tj);

int get_cpu_target_offset(void)
{
	return cpu_target_offset;
}
EXPORT_SYMBOL(get_cpu_target_offset);

/*add for DLPT*/
int tscpu_get_min_cpu_pwr(void)
{
	return MINIMUM_CPU_POWER;
}
EXPORT_SYMBOL(tscpu_get_min_cpu_pwr);

int tscpu_get_min_gpu_pwr(void)
{
	return MINIMUM_GPU_POWER;
}
EXPORT_SYMBOL(tscpu_get_min_gpu_pwr);

#if defined(THERMAL_VPU_SUPPORT)
int tscpu_get_min_vpu_pwr(void)
{
#if defined(THERMAL_APU_UNLIMIT)
	if (cl_get_apu_status() == 1)
		return MAXIMUM_VPU_POWER;
	else
		return MINIMUM_VPU_POWER;
#else
	return MINIMUM_VPU_POWER;
#endif
}
EXPORT_SYMBOL(tscpu_get_min_vpu_pwr);
#endif

#if defined(THERMAL_MDLA_SUPPORT)
int tscpu_get_min_mdla_pwr(void)
{
#if defined(THERMAL_APU_UNLIMIT)
	if (cl_get_apu_status() == 1)
		return MAXIMUM_MDLA_POWER;
	else
		return MINIMUM_MDLA_POWER;
#else
	return MINIMUM_MDLA_POWER;
#endif
}
EXPORT_SYMBOL(tscpu_get_min_mdla_pwr);
#endif

#if CONTINUOUS_TM
/**
 * @brief update cATM+ ttj control loop parameters
 * everytime related parameters are changed, we need to recalculate.
 * from thermal config: MAX_TARGET_TJ, STEADY_TARGET_TJ,
 * MIN_TTJ, TRIP_TPCB...etc
 * cATM+'s parameters: K_SUM_TT_HIGH, K_SUM_TT_LOW
 */

struct CATM_T thermal_atm_t;

static void catmplus_update_params(void)
{

	int ret = 0;

	thermal_atm_t.t_catm_par.CATM_ON = ctm_on;
	thermal_atm_t.t_catm_par.K_TT = K_TT;
	thermal_atm_t.t_catm_par.K_SUM_TT_LOW = K_SUM_TT_LOW;
	thermal_atm_t.t_catm_par.K_SUM_TT_HIGH = K_SUM_TT_HIGH;
	thermal_atm_t.t_catm_par.MIN_SUM_TT = MIN_SUM_TT;
	thermal_atm_t.t_catm_par.MAX_SUM_TT = MAX_SUM_TT;
	thermal_atm_t.t_catm_par.MIN_TTJ = MIN_TTJ;
	thermal_atm_t.t_catm_par.CATMP_STEADY_TTJ_DELTA =
						CATMP_STEADY_TTJ_DELTA;


	thermal_atm_t.t_continuetm_par.STEADY_TARGET_TJ = STEADY_TARGET_TJ;
	thermal_atm_t.t_continuetm_par.MAX_TARGET_TJ = MAX_TARGET_TJ;
	thermal_atm_t.t_continuetm_par.TRIP_TPCB  =  TRIP_TPCB;
	thermal_atm_t.t_continuetm_par.STEADY_TARGET_TPCB  =
						STEADY_TARGET_TPCB;

	ret = wakeup_ta_algo(TA_CATMPLUS);
	/*tscpu_warn("catmplus_update_params : ret %d\n" , ret);*/
}

#endif

#if PRECISE_HYBRID_POWER_BUDGET
static int _get_current_gpu_power(void)
{
	unsigned int cur_gpu_freq = mt_gpufreq_get_cur_freq();
	unsigned int cur_gpu_power = 0;
	int i = gpu_max_opp;

	if (mtk_gpu_power != NULL) {
		for (; i < Num_of_GPU_OPP; i++)
			if (mtk_gpu_power[i].gpufreq_khz == cur_gpu_freq)
				cur_gpu_power = mtk_gpu_power[i].gpufreq_power;
	}

	return (int) cur_gpu_power;
}

#if 0
static void reset_gpu_power_history(void)
{
	int i = 0;
	/*	Be careful when this can be invoked and error values. */
	unsigned int max_gpu_power = mt_gpufreq_get_max_power();

	if (gpu_power_sma_len > MAX_GPU_POWER_SMA_LEN)
		gpu_power_sma_len = MAX_GPU_POWER_SMA_LEN;

	for (i = 0; i < MAX_GPU_POWER_SMA_LEN; i++)
		gpu_power_history[i] = max_gpu_power;

	gpu_power_history_idx = 0;
}

/* we'll calculate SMA for gpu power,
 * but the output will still be aligned to OPP
 */
static int adjust_gpu_power(int power)
{
	int i, total = 0, sma_power;

	/* FIXME: debug only, this check should be
	 * moved to some setter functions.
	 *	or deleted if we don't want sma_len is changeable during runtime
	 */
	/*
	 *if (gpu_power_sma_len > MAX_GPU_POWER_SMA_LEN)
	 *	gpu_power_sma_len = MAX_GPU_POWER_SMA_LEN;
	 */

	if (power == 0)
		power = MAXIMUM_GPU_POWER;

	gpu_power_history[gpu_power_history_idx] = power;
	for (i = 0; i < gpu_power_sma_len; i++)
		total += gpu_power_history[i];

	gpu_power_history_idx = (gpu_power_history_idx + 1) % gpu_power_sma_len;
	sma_power = total / gpu_power_sma_len;

	for (i = gpu_max_opp; i < Num_of_GPU_OPP; i++) {
		if (mtk_gpu_power[i].gpufreq_power <= sma_power)
			break;
	}

	if (i >= Num_of_GPU_OPP)
		power = MINIMUM_GPU_POWER;
	else
		power = MAX(MINIMUM_GPU_POWER,
				(int)mtk_gpu_power[i].gpufreq_power);

	return power;
}
#endif
#endif

/*
 *Pass ATM total power budget to EARA for C/G/... allocation
 *ATM follow up if ERAR bypass
 */
static int EARA_handled(int total_power)
{
#if defined(EARA_THERMAL_SUPPORT)
	int ret = 0;
	int total_power_eara;

#if defined(CATM_TPCB_EXTEND)
	if (!g_turbo_bin)
		return 0;
#endif
#if defined(THERMAL_APU_UNLIMIT)
	if (cl_get_apu_status() == 1) {/*APU hint*/
		total_power = 0;/*let EARA unlimit VPU/MDLA freq*/
	}
#endif

#if defined(THERMAL_VPU_SUPPORT) && defined(THERMAL_MDLA_SUPPORT)
	if (total_power == 0)
		total_power_eara = 0;
	else if (is_cpu_power_unlimit())
		total_power_eara = total_power +
			MAXIMUM_VPU_POWER + MAXIMUM_MDLA_POWER;
	else
		total_power_eara = total_power +
			MINIMUM_VPU_POWER + MINIMUM_MDLA_POWER;
	ret = mtk_eara_thermal_pb_handle(total_power_eara,
		MAXIMUM_CPU_POWER, MAXIMUM_GPU_POWER,
		MAXIMUM_VPU_POWER, MAXIMUM_MDLA_POWER);
#else
	total_power_eara = total_power;
	ret = mtk_eara_thermal_pb_handle(total_power_eara,
		MAXIMUM_CPU_POWER, MAXIMUM_GPU_POWER, -1, -1);
#endif
		return ret;

#else
		return 0;
#endif
}

static int P_adaptive(int total_power, unsigned int gpu_loading)
{
	/*
	 *Here the gpu_power is the gpu power limit for the next interval,
	 *not exactly what gpu power selected by GPU DVFS
	 * But the ground rule is real gpu power should always
	 * under gpu_power for the same time interval
	 */
	static int cpu_power = 0, gpu_power;
	static int last_cpu_power = 0, last_gpu_power;
#if defined(THERMAL_VPU_SUPPORT)
	static int vpu_power, last_vpu_power;
#endif
#if defined(THERMAL_MDLA_SUPPORT)
	static int mdla_power, last_mdla_power;
#endif
#if defined(DDR_STRESS_WORKAROUND)
	static int opp0_cool;
#endif

	last_cpu_power = cpu_power;
	last_gpu_power = gpu_power;
#if defined(THERMAL_VPU_SUPPORT)
	last_vpu_power = vpu_power;
#endif
#if defined(THERMAL_MDLA_SUPPORT)
	last_mdla_power = mdla_power;
#endif
	g_total_power = total_power;

	if (EARA_handled(total_power) || (total_power == 0)) {
		cpu_power = gpu_power = 0;
#if defined(THERMAL_VPU_SUPPORT)
		vpu_power = 0;
#endif
#if defined(THERMAL_MDLA_SUPPORT)
		mdla_power = 0;
#endif

#if THERMAL_HEADROOM
		if (thp_max_cpu_power != 0)
			set_adaptive_cpu_power_limit(
				(unsigned int)MAX(
					thp_max_cpu_power, MINIMUM_CPU_POWER));
		else
			set_adaptive_cpu_power_limit(0);
#else
		set_adaptive_cpu_power_limit(0);
#endif
		set_adaptive_gpu_power_limit(0);
#if defined(THERMAL_VPU_SUPPORT)
		set_adaptive_vpu_power_limit(0);
#endif
#if defined(THERMAL_MDLA_SUPPORT)
		set_adaptive_mdla_power_limit(0);
#endif
#if (CONFIG_THERMAL_AEE_RR_REC == 1)
		aee_rr_rec_thermal_ATM_status(ATM_DONE);
#endif
		return 0;
	}

	if (total_power <= MINIMUM_TOTAL_POWER) {
		cpu_power = MINIMUM_CPU_POWER;
		gpu_power = MINIMUM_GPU_POWER;
	} else if (total_power >= MAXIMUM_TOTAL_POWER) {
		cpu_power = MAXIMUM_CPU_POWER;
		gpu_power = MAXIMUM_GPU_POWER;
	} else {
		if (mtk_gpu_power != NULL) {
			int max_allowed_gpu_power =
				MIN((total_power - MINIMUM_CPU_POWER),
							MAXIMUM_GPU_POWER);

			int max_gpu_power = (int) mt_gpufreq_get_max_power();
			int highest_possible_gpu_power =
				(max_allowed_gpu_power > max_gpu_power) ?
							(max_gpu_power+1) : -1;

			/* int highest_possible_gpu_power_idx = 0; */
			int i = gpu_max_opp;

			unsigned int cur_gpu_freq = mt_gpufreq_get_cur_freq();
			/* int cur_idx = 0; */
			unsigned int cur_gpu_power = 0;
			unsigned int next_lower_gpu_power = 0;

			/* get GPU highest possible power and index and
			 * current power and index and next lower power
			 */
			for (; i < Num_of_GPU_OPP; i++) {
				if ((mtk_gpu_power[i].gpufreq_power <=
					max_allowed_gpu_power) &&
					(-1 == highest_possible_gpu_power)) {
					/* choose OPP with power "<=" limit */
					highest_possible_gpu_power =
					mtk_gpu_power[i].gpufreq_power + 1;
					/* highest_possible_gpu_power_idx = i;*/
				}

				if (mtk_gpu_power[i].gpufreq_khz ==
				cur_gpu_freq) {
					/* choose OPP with power "<=" limit */
					next_lower_gpu_power = cur_gpu_power
					= (mtk_gpu_power[i].gpufreq_power + 1);
					/* cur_idx = i; */

					if ((i != Num_of_GPU_OPP - 1) &&
					(mtk_gpu_power[i + 1].gpufreq_power
					>= MINIMUM_GPU_POWER)) {
					/* choose OPP with power "<=" limit */
						next_lower_gpu_power =
							mtk_gpu_power[i + 1]
							.gpufreq_power + 1;
					}
				}
			}

			/* decide GPU power limit by loading */
			if (gpu_loading > GPU_L_H_TRIP) {
				gpu_power = highest_possible_gpu_power;
			} else if (gpu_loading <= GPU_L_L_TRIP) {
				gpu_power = MIN(next_lower_gpu_power,
						highest_possible_gpu_power);

				gpu_power = MAX(gpu_power, MINIMUM_GPU_POWER);
			} else {
				gpu_power = MIN(highest_possible_gpu_power,
								cur_gpu_power);

				gpu_power = MAX(gpu_power, MINIMUM_GPU_POWER);
			}
		}  else {
			gpu_power = 0;
		}

		cpu_power = MIN((total_power - gpu_power), MAXIMUM_CPU_POWER);
	}

#if defined(DDR_STRESS_WORKAROUND)
	if (tscpu_g_curr_temp > 70000) {
#if defined(CATM_TPCB_EXTEND)
		if ((mt_ppm_thermal_get_cur_power() >=
			mt_ppm_thermal_get_max_power()) ||
			(g_turbo_bin &&
			(mt_ppm_thermal_get_cur_power() >=
			mt_ppm_thermal_get_power_big_max_opp(1))))
#else
		if (mt_ppm_thermal_get_cur_power() >=
			mt_ppm_thermal_get_max_power())
#endif
			opp0_cool = 1;
	} else if (tscpu_g_curr_temp < 65000)
		opp0_cool = 0;

#if defined(CATM_TPCB_EXTEND)
	if ((g_turbo_bin) && (STEADY_TARGET_TPCB >= 58000))
		opp0_cool = 0;

	if (g_turbo_bin && (opp0_cool)) {
		if (cpu_power > mt_ppm_thermal_get_power_big_max_opp(1))
			cpu_power = mt_ppm_thermal_get_power_big_max_opp(1) - 5;
	} else if (opp0_cool)
		cpu_power -= 5;

#else
	if (opp0_cool)
		cpu_power -= 5;
#endif
#endif

#if defined(THERMAL_VPU_SUPPORT) && defined(THERMAL_MDLA_SUPPORT)
	g_delta_power = g_delta_power / 2;
#endif

#if defined(THERMAL_VPU_SUPPORT)
	if (total_power <= MINIMUM_TOTAL_POWER)
		vpu_power = MAX(last_vpu_power + g_delta_power,
			MINIMUM_VPU_POWER);
	else
		vpu_power = MAXIMUM_VPU_POWER;
#endif

#if defined(THERMAL_MDLA_SUPPORT)
	if (total_power <= MINIMUM_TOTAL_POWER)
		mdla_power = MAX(last_mdla_power + g_delta_power,
			MINIMUM_MDLA_POWER);
	else
		mdla_power = MAXIMUM_MDLA_POWER;
#endif

#if 0
	/* TODO: check if this segment can be used in original design
	 * GPU SMA
	 */
	if ((gpu_power_sma_len > 1) && (tscpu_atm == 3)) {
		total_power = gpu_power + cpu_power;
		gpu_power = adjust_gpu_power(gpu_power);
		cpu_power = total_power - gpu_power;
	}
#endif

	if (cpu_power != last_cpu_power)
		set_adaptive_cpu_power_limit(cpu_power);

	if ((gpu_power != last_gpu_power) && (mtk_gpu_power != NULL)) {
		/* Work-around for unsync GPU power table problem 1. */
		if (gpu_power >= mtk_gpu_power[gpu_max_opp].gpufreq_power)
			set_adaptive_gpu_power_limit(0);
		else
			set_adaptive_gpu_power_limit(gpu_power);
	}

	tscpu_dprintk("%s cpu %d, gpu %d\n", __func__, cpu_power, gpu_power);

#if defined(THERMAL_VPU_SUPPORT)
	/*APU hint*/
#if defined(THERMAL_APU_UNLIMIT)
	if (cl_get_apu_status() == 1) {
		set_adaptive_vpu_power_limit(0);
	} else {
		if (vpu_power != last_vpu_power) {
			if (vpu_power >= MAXIMUM_VPU_POWER)
				set_adaptive_vpu_power_limit(0);
			else
				set_adaptive_vpu_power_limit(vpu_power);
		}
	}
#else
	if (vpu_power != last_vpu_power) {
		if (vpu_power >= MAXIMUM_VPU_POWER)
			set_adaptive_vpu_power_limit(0);
		else
			set_adaptive_vpu_power_limit(vpu_power);
	}
#endif
	tscpu_dprintk("%s vpu %d\n",
		__func__, vpu_power);
#endif
#if defined(THERMAL_MDLA_SUPPORT)
	/*APU hint*/
#if defined(THERMAL_APU_UNLIMIT)
	if (cl_get_apu_status() == 1) {
		set_adaptive_mdla_power_limit(0);
	} else {
		if (mdla_power != last_mdla_power) {
			if (mdla_power >= MAXIMUM_MDLA_POWER)
				set_adaptive_mdla_power_limit(0);
			else
				set_adaptive_mdla_power_limit(mdla_power);
		}
	}
#else
	if (mdla_power != last_mdla_power) {
		if (mdla_power >= MAXIMUM_MDLA_POWER)
			set_adaptive_mdla_power_limit(0);
		else
			set_adaptive_mdla_power_limit(mdla_power);
	}
#endif
	tscpu_dprintk("%s mdla %d\n",
		__func__, mdla_power);
#endif

#if (CONFIG_THERMAL_AEE_RR_REC == 1)
	aee_rr_rec_thermal_ATM_status(ATM_DONE);
#endif
	return 0;
}

#if PRECISE_HYBRID_POWER_BUDGET
static int __phpb_dynamic_theta(int max_theta)
{
	int theta;
	/*	temp solution as CATM
	 *	FIXME: API? how to get tj trip?
	 */
	int tj_trip = TARGET_TJS[0];

	if (tj_trip >= TARGET_TJ)
		theta = max_theta;
	else if (TARGET_TJ >= MAX_TARGET_TJ)
		theta = phpb_theta_min;
	else {
		theta = max_theta - (TARGET_TJ - tj_trip) *
			(max_theta - phpb_theta_min) /
			(MAX_TARGET_TJ - tj_trip);
	}

	return theta;
}

/**
 * TODO: target_tj is adjusted by catmv2, therefore dynamic_theta would not
 * changed frequently.
 */
static int __phpb_calc_delta(int curr_temp, int prev_temp, int phpb_param_idx)
{
	struct phpb_param *p = &phpb_params[phpb_param_idx];
	int tt = TARGET_TJ - curr_temp;
	int tp = prev_temp - curr_temp;
	int delta_power = 0, delta_power_tt, delta_power_tp;

	/* *2 is to cover Tj jump betwen [TTJ-tj_stable_range,
	 * TTJ+tj_stable_range]
	 */
	if ((abs(tt) > tj_stable_range) || (abs(tp) > (tj_stable_range * 2))) {
		delta_power_tt = tt / p->tt;
		delta_power_tp = tp / p->tp;
		/* When Tj is rising, double power cut. */
		if (delta_power_tp < 0)
			delta_power_tp *= 2;
		delta_power = (delta_power_tt + delta_power_tp) /
			      __phpb_dynamic_theta(phpb_theta_max);
	}
	if ((tt < 0) && (curr_temp < TARGET_TJ)) {
		delta_power = 0;
		tscpu_dprintk("%s Wrong  TARGET_TJ\n", __func__);
	}

	return delta_power;
}

static int phpb_calc_delta(int curr_temp, int prev_temp)
{
	int delta, param_idx;

	if (tscpu_curr_cpu_temp >= tscpu_curr_gpu_temp)
		param_idx = PHPB_PARAM_CPU;
	else
		param_idx = PHPB_PARAM_GPU;

	delta = __phpb_calc_delta(curr_temp, prev_temp, param_idx);

	return delta;
}

/* get current power from current opp, real value, does not set minimum */
int clatm_get_curr_opp_power(void)
{
	int cpu_power = 0, gpu_power = 0;

	cpu_power = (int) mt_ppm_thermal_get_cur_power();
	gpu_power = _get_current_gpu_power();

	tscpu_dprintk("%s cpu power=%d gpu power=%d\n",
		__func__, cpu_power, gpu_power);

	return cpu_power + gpu_power;
}

/* calculated total power based on current opp */
static int get_total_curr_power(void)
{
	int cpu_power = 0, gpu_power = 0;

#ifdef ATM_USES_PPM
	/* choose OPP with power "<=" limit */
	cpu_power = (int) mt_ppm_thermal_get_cur_power() + 1;
#else
	/* maybe you should disable PRECISE_HYBRID_POWER_BUDGET
	 * if current cpu power unavailable .
	 */
	cpu_power = 0;
#endif
	/* avoid idle power too small to hurt another unit performance */
	if (cpu_power < MINIMUM_CPU_POWER)
		cpu_power = MINIMUM_CPU_POWER;

	/* choose OPP with power "<=" limit */
	gpu_power = _get_current_gpu_power() + 1;
	if (gpu_power < MINIMUM_GPU_POWER)
		gpu_power = MINIMUM_GPU_POWER;

	return cpu_power + gpu_power;
}

static int phpb_calc_total(int prev_total_power, long curr_temp, long prev_temp)
{
	/* total_power is new power limit, which curr_power is current power
	 * calculated based on current opp
	 */
	int delta_power, total_power, curr_power;
#if 0 /* Just use previous total power to avoid conflict with fpsgo */
	int tt = TARGET_TJ - curr_temp;
	int tp = prev_temp - curr_temp;
#endif

	delta_power = phpb_calc_delta(curr_temp, prev_temp);
#if defined(THERMAL_VPU_SUPPORT) || defined(THERMAL_MDLA_SUPPORT)
	g_delta_power = delta_power;
#endif
	if (delta_power == 0)
		return prev_total_power;

	curr_power = get_total_curr_power();

#if 0 /* Just use previous total power to avoid conflict with fpsgo */
	/* In some conditions, we will consider using current request power to
	 * avoid giving unlimit power budget.
	 * Temp. rising is large,  requset power is of course less than power
	 * limit (but it sometime goes over...)
	 */
	if (curr_power < prev_total_power
	&& ((-tt) >= tj_stable_range * 2
	|| (-tp) >= tj_stable_range * 4)) {
		tscpu_dprintk(
			"%s prev_temp %ld, curr_temp %ld, curr %d, delta %d\n",
			__func__, prev_temp, curr_temp, curr_power,
			delta_power);

		total_power = curr_power + delta_power;
	} else {
		tscpu_dprintk(
			"%s prev_temp %ld, curr_temp %ld, prev %d, delta %d\n",
			__func__, prev_temp, curr_temp, prev_total_power,
			delta_power);

		total_power = prev_total_power + delta_power;
	}
#else
	tscpu_dprintk(
	"%s prev_temp %ld, curr_temp %ld, prev %d, delta %d, curr %d\n",
	__func__, prev_temp, curr_temp, prev_total_power,
	delta_power, curr_power);

	total_power = prev_total_power + delta_power;
#endif

	total_power = clamp(total_power, MINIMUM_TOTAL_POWER,
						MAXIMUM_TOTAL_POWER);

	return total_power;
}

static int _adaptive_power_ppb
(long prev_temp, long curr_temp, unsigned int gpu_loading)
{
	static int triggered, total_power;
	int delta_power = 0;

	if (cl_dev_adp_cpu_state_active == 1) {
		tscpu_dprintk("%s %d %d %d %d %d %d %d\n", __func__,
				PACKAGE_THETA_JA_RISE, PACKAGE_THETA_JA_FALL,
				MINIMUM_BUDGET_CHANGE, MINIMUM_CPU_POWER,
				MAXIMUM_CPU_POWER, MINIMUM_GPU_POWER,
				MAXIMUM_GPU_POWER);

		/* Check if it is triggered */
		if (!triggered) {
			triggered = 1;
			total_power = phpb_calc_total(get_total_curr_power(),
							curr_temp, prev_temp);

			tscpu_dprintk(
				"%s triggered:0->1 Tp %ld, Tc %ld, TARGET_TJ %d, Pt %d\n",
				__func__, prev_temp, curr_temp, TARGET_TJ,
				total_power);

			return P_adaptive(total_power, gpu_loading);
		}

		/* Adjust total power budget if necessary */
		total_power = phpb_calc_total(total_power,
						curr_temp, prev_temp);
		/*	TODO: delta_power is not changed but printed. */
		tscpu_dprintk(
			"%s TARGET_TJ %d, delta_power %d, total_power %d\n",
			__func__, TARGET_TJ, delta_power, total_power);

		tscpu_dprintk("%s Tp %ld, Tc %ld, Pt %d\n", __func__,
					prev_temp, curr_temp, total_power);

		return P_adaptive(total_power, gpu_loading);
		/* end of cl_dev_adp_cpu_state_active == 1 */
	} else {
		if (triggered) {
			triggered = 0;
			tscpu_dprintk("%s Tp %ld, Tc %ld, Pt %d\n", __func__,
					prev_temp, curr_temp, total_power);

			if (curr_temp > current_ETJ)
				tscpu_printk("exit but Tc(%ld) > cetj(%d)\n",
					curr_temp, current_ETJ);

			return P_adaptive(0, 0);
#if THERMAL_HEADROOM
		} else {
			if (thp_max_cpu_power != 0)
				set_adaptive_cpu_power_limit(
					(unsigned int) MAX(thp_max_cpu_power,
							MINIMUM_CPU_POWER));
			else
				set_adaptive_cpu_power_limit(0);
		}
#else
		}
#endif
		/* reset_gpu_power_history(); */
	}

	return 0;
}
#endif

static int _adaptive_power
(long prev_temp, long curr_temp, unsigned int gpu_loading)
{
	static int triggered = 0, total_power;
	int delta_power = 0;

	if (cl_dev_adp_cpu_state_active == 1) {
		tscpu_dprintk("%s %d %d %d %d %d %d %d %d\n", __func__,
				FIRST_STEP_TOTAL_POWER_BUDGET,
				PACKAGE_THETA_JA_RISE, PACKAGE_THETA_JA_FALL,
				MINIMUM_BUDGET_CHANGE, MINIMUM_CPU_POWER,
				MAXIMUM_CPU_POWER, MINIMUM_GPU_POWER,
				MAXIMUM_GPU_POWER);

		/* Check if it is triggered */
		if (!triggered) {
			if (curr_temp < TARGET_TJ)
				return 0;

			triggered = 1;
			switch (tscpu_atm) {
			case 1:	/* FTL ATM v2 */
			case 2:	/* CPU_GPU_Weight ATM v2 */
#if MTKTSCPU_FAST_POLLING
				total_power =
				FIRST_STEP_TOTAL_POWER_BUDGET -
				((curr_temp - TARGET_TJ) * tt_ratio_high_rise +
				(curr_temp - prev_temp) * tp_ratio_high_rise) /
				(PACKAGE_THETA_JA_RISE * tscpu_cur_fp_factor);
#else
				total_power =
				FIRST_STEP_TOTAL_POWER_BUDGET -
				((curr_temp - TARGET_TJ) * tt_ratio_high_rise +
				(curr_temp - prev_temp) * tp_ratio_high_rise) /
				PACKAGE_THETA_JA_RISE;
#endif
				break;
			case 0:
			default:	/* ATM v1 */
				total_power = FIRST_STEP_TOTAL_POWER_BUDGET;
			}

			tscpu_dprintk("%s Tp %ld, Tc %ld, Pt %d\n", __func__,
					prev_temp, curr_temp, total_power);

			return P_adaptive(total_power, gpu_loading);
		}

		/* Adjust total power budget if necessary */
		switch (tscpu_atm) {
		case 1:	/* FTL ATM v2 */
		case 2:	/* CPU_GPU_Weight ATM v2 */
			if ((curr_temp >= TARGET_TJ_HIGH)
				&& (curr_temp > prev_temp)) {
#if MTKTSCPU_FAST_POLLING
				total_power -=
					MAX(((curr_temp - TARGET_TJ) *
					tt_ratio_high_rise +
					(curr_temp - prev_temp) *
					tp_ratio_high_rise) /
					(PACKAGE_THETA_JA_RISE *
					tscpu_cur_fp_factor),
					MINIMUM_BUDGET_CHANGE);
#else
				total_power -=
					MAX(((curr_temp - TARGET_TJ) *
					tt_ratio_high_rise +
					(curr_temp - prev_temp) *
					tp_ratio_high_rise) /
					PACKAGE_THETA_JA_RISE,
					MINIMUM_BUDGET_CHANGE);
#endif
			} else if ((curr_temp >= TARGET_TJ_HIGH)
				&& (curr_temp <= prev_temp)) {
#if MTKTSCPU_FAST_POLLING
				total_power -=
					MAX(((curr_temp - TARGET_TJ) *
					tt_ratio_high_fall -
					(prev_temp - curr_temp) *
					tp_ratio_high_fall) /
					(PACKAGE_THETA_JA_FALL *
					tscpu_cur_fp_factor),
					MINIMUM_BUDGET_CHANGE);
#else
				total_power -=
					MAX(((curr_temp - TARGET_TJ) *
					tt_ratio_high_fall -
					(prev_temp - curr_temp) *
					tp_ratio_high_fall) /
					PACKAGE_THETA_JA_FALL,
					MINIMUM_BUDGET_CHANGE);
#endif
			} else if ((curr_temp <= TARGET_TJ_LOW)
				&& (curr_temp > prev_temp)) {
#if MTKTSCPU_FAST_POLLING
				total_power +=
					MAX(((TARGET_TJ - curr_temp) *
					tt_ratio_low_rise -
					(curr_temp - prev_temp) *
					tp_ratio_low_rise) /
					(PACKAGE_THETA_JA_RISE *
							tscpu_cur_fp_factor),
					MINIMUM_BUDGET_CHANGE);
#else
				total_power +=
					MAX(((TARGET_TJ - curr_temp) *
					tt_ratio_low_rise -
					(curr_temp - prev_temp) *
					tp_ratio_low_rise) /
					PACKAGE_THETA_JA_RISE,
					MINIMUM_BUDGET_CHANGE);
#endif
			} else if ((curr_temp <= TARGET_TJ_LOW)
				&& (curr_temp <= prev_temp)) {
#if MTKTSCPU_FAST_POLLING
				total_power +=
					MAX(((TARGET_TJ - curr_temp) *
					tt_ratio_low_fall +
					(prev_temp - curr_temp) *
					tp_ratio_low_fall) /
					(PACKAGE_THETA_JA_FALL *
							tscpu_cur_fp_factor),
					MINIMUM_BUDGET_CHANGE);
#else
				total_power +=
					MAX(((TARGET_TJ - curr_temp) *
					tt_ratio_low_fall +
					(prev_temp - curr_temp) *
					tp_ratio_low_fall) /
					PACKAGE_THETA_JA_FALL,
					MINIMUM_BUDGET_CHANGE);
#endif
			}

			total_power = (total_power > MINIMUM_TOTAL_POWER) ?
					total_power : MINIMUM_TOTAL_POWER;

			total_power = (total_power < MAXIMUM_TOTAL_POWER) ?
					total_power : MAXIMUM_TOTAL_POWER;

			break;

		case 0:
		default:	/* ATM v1 */
			if ((curr_temp > TARGET_TJ_HIGH)
			&& (curr_temp >= prev_temp)) {
#if MTKTSCPU_FAST_POLLING
				delta_power = (curr_temp - prev_temp) /
							(PACKAGE_THETA_JA_RISE *
							tscpu_cur_fp_factor);
#else
				delta_power = (curr_temp - prev_temp) /
							PACKAGE_THETA_JA_RISE;
#endif
				if (prev_temp > TARGET_TJ_HIGH) {
					delta_power =
					    (delta_power >
						MINIMUM_BUDGET_CHANGE) ?
							delta_power :
					    MINIMUM_BUDGET_CHANGE;
				}

				total_power -= delta_power;
				total_power = (total_power >
						MINIMUM_TOTAL_POWER) ?
							total_power :
							MINIMUM_TOTAL_POWER;
			}

			if ((curr_temp < TARGET_TJ_LOW)
			&& (curr_temp <= prev_temp)) {
#if MTKTSCPU_FAST_POLLING
				delta_power = (prev_temp - curr_temp) /
							(PACKAGE_THETA_JA_FALL *
							tscpu_cur_fp_factor);
#else
				delta_power = (prev_temp - curr_temp) /
							PACKAGE_THETA_JA_FALL;
#endif
				if (prev_temp < TARGET_TJ_LOW) {
					delta_power =
					    (delta_power >
						MINIMUM_BUDGET_CHANGE) ?
						delta_power :
					    MINIMUM_BUDGET_CHANGE;
				}

				total_power += delta_power;
				total_power = (total_power <
						MAXIMUM_TOTAL_POWER) ?
						total_power :
						MAXIMUM_TOTAL_POWER;
			}
			break;
		}

		tscpu_dprintk("%s Tp %ld, Tc %ld, Pt %d\n", __func__,
							prev_temp, curr_temp,
			      total_power);

		return P_adaptive(total_power, gpu_loading);
	}
#if CONTINUOUS_TM
	else if ((cl_dev_adp_cpu_state_active == 1)
	&& (ctm_on) && (curr_temp < current_ETJ)) {
		tscpu_printk("CTM exit curr_temp %d cetj %d\n",
						TARGET_TJ, current_ETJ);
		/* even cooler not exit,
		 * when CTM is on and current Tj < current_ETJ, leave ATM
		 */
		if (triggered) {
			triggered = 0;
			tscpu_dprintk("%s Tp %ld, Tc %ld, Pt %d\n", __func__,
							prev_temp, curr_temp,
				      total_power);
			return P_adaptive(0, 0);
		}
#if THERMAL_HEADROOM
		else {
			if (thp_max_cpu_power != 0)
				set_adaptive_cpu_power_limit((unsigned int)
					MAX(thp_max_cpu_power,
					MINIMUM_CPU_POWER));
			else
				set_adaptive_cpu_power_limit(0);
		}
#endif
	}
#endif
	else {
		if (triggered) {
			triggered = 0;
			tscpu_dprintk("%s Tp %ld, Tc %ld, Pt %d\n", __func__,
							prev_temp, curr_temp,
				      total_power);

			return P_adaptive(0, 0);
		}
#if THERMAL_HEADROOM
		else {
			if (thp_max_cpu_power != 0)
				set_adaptive_cpu_power_limit((unsigned int)
					MAX(thp_max_cpu_power,
					MINIMUM_CPU_POWER));
			else
				set_adaptive_cpu_power_limit(0);
		}
#endif
	}

	return 0;
}

static int decide_ttj(void)
{
	int i = 0;
	int active_cooler_id = -1;
	int ret = 117000;	/* highest allowable TJ */
	int temp_cl_dev_adp_cpu_state_active = 0;
	int cur_min_gpu_pwr = (int)mt_gpufreq_get_min_power() + 1;
	int cur_max_gpu_pwr = (int)mt_gpufreq_get_max_power() + 1;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	static int prev_ttj = 85000;

	prev_ttj = TARGET_TJ;
#endif
#endif

	for (; i < MAX_CPT_ADAPTIVE_COOLERS; i++) {
		if (cl_dev_adp_cpu_state[i]) {
			ret = MIN(ret, TARGET_TJS[i]);
			temp_cl_dev_adp_cpu_state_active = 1;

			if (ret == TARGET_TJS[i])
				active_cooler_id = i;
		}
	}
	cl_dev_adp_cpu_state_active = temp_cl_dev_adp_cpu_state_active;
	TARGET_TJ = ret;
#if CONTINUOUS_TM
	if (ctm_on) {
		int curr_tpcb = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_AP);

		if (ctm_on == 1) {
			TARGET_TJ = MIN(MAX_TARGET_TJ,
					MAX(STEADY_TARGET_TJ,
						(COEF_AE - COEF_BE * (curr_tpcb
								/ 1000))));

		} else if (ctm_on == 2) {
			/* +++ cATM+ +++ */
			TARGET_TJ = ta_get_ttj();
			/* --- cATM+ --- */
		}

		current_ETJ = MIN(MAX_EXIT_TJ,
				MAX(STEADY_EXIT_TJ,
					(COEF_AX - COEF_BX * (curr_tpcb
								/ 1000))));

		/* tscpu_printk("cttj %d cetj %d tpcb %d\n",
		 *			TARGET_TJ, current_ETJ, curr_tpcb);
		 */
	}
#endif
	cpu_target_tj = TARGET_TJ;
#if CONTINUOUS_TM
	cpu_target_offset = TARGET_TJ - current_ETJ;
#endif

	TARGET_TJ_HIGH = TARGET_TJ + 1000;
	TARGET_TJ_LOW = TARGET_TJ - 1000;

	if (active_cooler_id >= 0
	&& MAX_CPT_ADAPTIVE_COOLERS > active_cooler_id) {

		PACKAGE_THETA_JA_RISE =
				PACKAGE_THETA_JA_RISES[active_cooler_id];

		PACKAGE_THETA_JA_FALL =
				PACKAGE_THETA_JA_FALLS[active_cooler_id];

		MINIMUM_CPU_POWER = MINIMUM_CPU_POWERS[active_cooler_id];
		MAXIMUM_CPU_POWER = MAXIMUM_CPU_POWERS[active_cooler_id];

		/*	get GPU min/max power from GPU DVFS should be
		 *	done when configuring ATM instead of decide_ttj
		 */
#if 0
		{
			MAXIMUM_GPU_POWER = (int)mt_gpufreq_get_max_power();
			MINIMUM_GPU_POWER = (int)mt_gpufreq_get_min_power();
			tscpu_printk(
				"%s: MAXIMUM_GPU_POWER=%d, MINIMUM_GPU_POWER=%d\n",
				__func__, MAXIMUM_GPU_POWER, MINIMUM_GPU_POWER);
		}
#else
		MINIMUM_GPU_POWER = MINIMUM_GPU_POWERS[active_cooler_id];
		if (!is_max_gpu_power_specified[active_cooler_id])
			MAXIMUM_GPU_POWERS[active_cooler_id] = cur_max_gpu_pwr;
		else if (MAXIMUM_GPU_POWERS[active_cooler_id] < cur_min_gpu_pwr)
			MAXIMUM_GPU_POWERS[active_cooler_id] = cur_min_gpu_pwr;
		MAXIMUM_GPU_POWER = MAXIMUM_GPU_POWERS[active_cooler_id];
#endif
		MINIMUM_TOTAL_POWER = MINIMUM_CPU_POWER + MINIMUM_GPU_POWER;
		MAXIMUM_TOTAL_POWER = MAXIMUM_CPU_POWER + MAXIMUM_GPU_POWER;

		FIRST_STEP_TOTAL_POWER_BUDGET =
			FIRST_STEP_TOTAL_POWER_BUDGETS[active_cooler_id];

		MINIMUM_BUDGET_CHANGE =
			MINIMUM_BUDGET_CHANGES[active_cooler_id];

	} else {
		MINIMUM_CPU_POWER = MINIMUM_CPU_POWERS[0];
		MAXIMUM_CPU_POWER = MAXIMUM_CPU_POWERS[0];
		MINIMUM_GPU_POWER = MINIMUM_GPU_POWERS[0];
		if (!is_max_gpu_power_specified[0])
			MAXIMUM_GPU_POWERS[0] = cur_max_gpu_pwr;
		else if (MAXIMUM_GPU_POWERS[0] < cur_min_gpu_pwr)
			MAXIMUM_GPU_POWERS[0] = cur_min_gpu_pwr;
		MAXIMUM_GPU_POWER = MAXIMUM_GPU_POWERS[0];
	}

#if THERMAL_HEADROOM
	MAXIMUM_CPU_POWER -= p_Tpcb_correlation *
				MAX((bts_cur_temp - Tpcb_trip_point), 0) / 1000;

	/* tscpu_printk("max_cpu_pwr %d %d\n",
	 *		bts_cur_temp, MAXIMUM_CPU_POWER);
	 */

	/* TODO: use 0 as current P */
	thp_max_cpu_power = (thp_threshold_tj - tscpu_read_curr_temp) *
						thp_p_tj_correlation / 1000 + 0;

	if (thp_max_cpu_power != 0)
		MAXIMUM_CPU_POWER = MIN(MAXIMUM_CPU_POWER, thp_max_cpu_power);

	MAXIMUM_CPU_POWER = MAX(MAXIMUM_CPU_POWER, MINIMUM_CPU_POWER);

	/* tscpu_printk("thp max_cpu_pwr %d %d\n",
	 *			thp_max_cpu_power, MAXIMUM_CPU_POWER);
	 */
#endif

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	if (prev_ttj != TARGET_TJ)
		atm_update_ttj_to_sspm();
	if (atm_prev_active_atm_cl_id != active_cooler_id) {
		atm_prev_active_atm_cl_id = active_cooler_id;
		atm_update_atm_param_to_sspm();
	}
#endif
#endif

	return ret;
}
#endif

#if defined(CATM_TPCB_EXTEND)
#define CPUFREQ_SEG_CODE_IDX_0 7

static void mtk_thermal_get_turbo(void)
{

	g_turbo_bin =
		(get_devinfo_with_index(CPUFREQ_SEG_CODE_IDX_0) >> 3) & 0x1;

	tscpu_printk("%s: turbo: %d\n", __func__, g_turbo_bin);
}
#endif

#if CPT_ADAPTIVE_AP_COOLER
static int adp_cpu_get_max_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("adp_cpu_get_max_state\n"); */
	*state = 1;
	return 0;
}

static int adp_cpu_get_cur_state
(struct thermal_cooling_device *cdev, unsigned long *state)
{
	/* tscpu_dprintk("adp_cpu_get_cur_state\n"); */
	*state = cl_dev_adp_cpu_state[(cdev->type[13] - '0')];
	/* *state = cl_dev_adp_cpu_state; */
	return 0;
}

static int adp_cpu_set_cur_state
(struct thermal_cooling_device *cdev, unsigned long state)
{
	int ttj = 117000;
	unsigned int prev_active_state = cl_dev_adp_cpu_state_active;

	cl_dev_adp_cpu_state[(cdev->type[13] - '0')] = state;

	/* TODO: no exit point can be obtained in mtk_ts_cpu.c */
	ttj = decide_ttj();

	/* tscpu_dprintk("adp_cpu_set_cur_state[%d] =%d, ttj=%d\n",
	 *				(cdev->type[13] - '0'), state, ttj);
	 */

	if (prev_active_state && !cl_dev_adp_cpu_state_active
		&& tscpu_g_curr_temp > current_ETJ)
		tscpu_printk("[Warning] active 1 -> 0 but Tc(%d) > cetj(%d)\n",
			tscpu_g_curr_temp, current_ETJ);


#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	if (atm_sspm_enabled)
		goto exit;
#endif
#endif

#ifndef FAST_RESPONSE_ATM
	if (active_adp_cooler == (int)(cdev->type[13] - '0')) {
		/* = (NULL == mtk_thermal_get_gpu_loading_fp) ?
		 *			0 : mtk_thermal_get_gpu_loading_fp();
		 */
		unsigned int gpu_loading;

		if (!mtk_get_gpu_loading(&gpu_loading))
			gpu_loading = 0;

		_adaptive_power_calc(tscpu_g_prev_temp, tscpu_g_curr_temp,
						(unsigned int) gpu_loading);
	}
#endif

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
exit:
#endif
#endif
	return 0;
}
#endif

#if CPT_ADAPTIVE_AP_COOLER
static struct thermal_cooling_device_ops mtktscpu_cooler_adp_cpu_ops = {
	.get_max_state = adp_cpu_get_max_state,
	.get_cur_state = adp_cpu_get_cur_state,
	.set_cur_state = adp_cpu_set_cur_state,
};
#endif

#if CPT_ADAPTIVE_AP_COOLER
static int tscpu_read_atm_setting(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < MAX_CPT_ADAPTIVE_COOLERS; i++) {
		seq_printf(m, "%s%02d\n", adaptive_cooler_name, i);
		seq_printf(m, " first_step = %d\n",
					FIRST_STEP_TOTAL_POWER_BUDGETS[i]);

		seq_printf(m, " theta rise = %d\n",
					PACKAGE_THETA_JA_RISES[i]);

		seq_printf(m, " theta fall = %d\n",
					PACKAGE_THETA_JA_FALLS[i]);

		seq_printf(m, " min_budget_change = %d\n",
					MINIMUM_BUDGET_CHANGES[i]);

		seq_printf(m, " m cpu = %d\n", MINIMUM_CPU_POWERS[i]);
		seq_printf(m, " M cpu = %d\n", MAXIMUM_CPU_POWERS[i]);
		seq_printf(m, " m gpu = %d\n", MINIMUM_GPU_POWERS[i]);
		seq_printf(m, " M gpu = %d\n", MAXIMUM_GPU_POWERS[i]);
	}

#if defined(THERMAL_VPU_SUPPORT)
	seq_printf(m, " m vpu = %d\n", MINIMUM_VPU_POWER);
	seq_printf(m, " M vpu = %d\n", MAXIMUM_VPU_POWER);
#endif
#if defined(THERMAL_MDLA_SUPPORT)
	seq_printf(m, " m mdla = %d\n", MINIMUM_MDLA_POWER);
	seq_printf(m, " M mdla = %d\n", MAXIMUM_MDLA_POWER);
#endif

	return 0;
}

static ssize_t tscpu_write_atm_setting
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	/* char arg_name[32] = {0}; */
	/* int arg_val = 0; */
	int len = 0;

	int i_id = -1, i_first_step = -1, i_theta_r = -1, i_theta_f = -1,
		i_budget_change = -1, i_min_cpu_pwr = -1, i_max_cpu_pwr = -1,
		i_min_gpu_pwr = -1, i_max_gpu_pwr = -1;

#if defined(THERMAL_VPU_SUPPORT)
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	MINIMUM_VPU_POWER = vpu_power_table[APU_OPP_NUM - 1].power;
	MAXIMUM_VPU_POWER = vpu_power_table[APU_OPP_0].power;
#else
	MINIMUM_VPU_POWER = vpu_power_table[VPU_OPP_NUM - 1].power;
	MAXIMUM_VPU_POWER = vpu_power_table[VPU_OPP_0].power;
#endif
#endif
#if defined(THERMAL_MDLA_SUPPORT)
#ifdef CONFIG_MTK_APUSYS_SUPPORT
	MINIMUM_MDLA_POWER = mdla_power_table[APU_OPP_NUM - 1].power;
	MAXIMUM_MDLA_POWER = mdla_power_table[APU_OPP_0].power;
#else
	MINIMUM_MDLA_POWER = mdla_power_table[MDLA_OPP_NUM - 1].power;
	MAXIMUM_MDLA_POWER = mdla_power_table[MDLA_OPP_0].power;
#endif
#endif

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d %d %d %d %d %d %d",
		&i_id, &i_first_step, &i_theta_r, &i_theta_f,
		&i_budget_change, &i_min_cpu_pwr, &i_max_cpu_pwr,
		&i_min_gpu_pwr, &i_max_gpu_pwr) >= 9) {

		tscpu_printk(
			"%s input %d %d %d %d %d %d %d %d %d\n", __func__,
			i_id, i_first_step,
			i_theta_r, i_theta_f, i_budget_change,
			i_min_cpu_pwr, i_max_cpu_pwr, i_min_gpu_pwr,
			i_max_gpu_pwr);

		if (i_id >= 0 && i_id < MAX_CPT_ADAPTIVE_COOLERS) {
			if (i_first_step > 0)
				FIRST_STEP_TOTAL_POWER_BUDGETS[i_id] =
								i_first_step;

			else {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT,
						"tscpu_write_atm_setting",
						"Wrong thermal policy");
				#endif
			}
			if (i_theta_r > 0)
				PACKAGE_THETA_JA_RISES[i_id] = i_theta_r;
			else {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT,
						"tscpu_write_atm_setting",
						"Wrong thermal policy");
				#endif
			}
			if (i_theta_f > 0)
				PACKAGE_THETA_JA_FALLS[i_id] = i_theta_f;
			else {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT,
						"tscpu_write_atm_setting",
						"Wrong thermal policy");
				#endif
			}
			if (i_budget_change >= 0)
				MINIMUM_BUDGET_CHANGES[i_id] = i_budget_change;
			else {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT,
						"tscpu_write_atm_setting",
						"Wrong thermal policy");
				#endif
			}
			if (i_min_cpu_pwr > 0)
				MINIMUM_CPU_POWERS[i_id] = i_min_cpu_pwr;
#ifdef ATM_USES_PPM
			else if (i_min_cpu_pwr == 0)
				MINIMUM_CPU_POWERS[i_id] =
					mt_ppm_thermal_get_min_power() + 1;
				/* choose OPP with power "<=" limit */
#endif
			else {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT,
						"tscpu_write_atm_setting",
						"Wrong thermal policy");
				#endif
			}
			if (i_max_cpu_pwr > 0)
				MAXIMUM_CPU_POWERS[i_id] = i_max_cpu_pwr;
#ifdef ATM_USES_PPM
			else if (i_max_cpu_pwr == 0)
				MAXIMUM_CPU_POWERS[i_id] =
					mt_ppm_thermal_get_max_power() + 1;
				/* choose OPP with power "<=" limit */
#endif
			else {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT,
						"tscpu_write_atm_setting",
						"Wrong thermal policy");
				#endif
			}
			if (i_min_gpu_pwr > 0) {
				/* choose OPP with power "<=" limit */
				int min_gpuopp_power =
					(int) mt_gpufreq_get_min_power() + 1;

				MINIMUM_GPU_POWERS[i_id] =
					MAX(i_min_gpu_pwr, min_gpuopp_power);

			} else if (i_min_gpu_pwr == 0)
				MINIMUM_GPU_POWERS[i_id] =
					(int) mt_gpufreq_get_min_power() + 1;
				/* choose OPP with power "<=" limit */
			else {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT,
						"tscpu_write_atm_setting",
						"Wrong thermal policy");
				#endif
			}

			if (i_max_gpu_pwr > 0) {
				/* choose OPP with power "<=" limit */
				int min_gpuopp_power =
					(int) mt_gpufreq_get_min_power() + 1;

				MAXIMUM_GPU_POWERS[i_id] =
					MAX(i_max_gpu_pwr, min_gpuopp_power);
				is_max_gpu_power_specified[i_id] = 1;
			} else if (i_max_gpu_pwr == 0) {
				/* choose OPP with power "<=" limit */
				MAXIMUM_GPU_POWERS[i_id] =
					(int) mt_gpufreq_get_max_power() + 1;
				is_max_gpu_power_specified[i_id] = 0;
			} else {
				#ifdef CONFIG_MTK_AEE_FEATURE
				aee_kernel_warning_api(__FILE__, __LINE__,
						DB_OPT_DEFAULT,
						"tscpu_write_atm_setting",
						"Wrong thermal policy");
				#endif
			}

			active_adp_cooler = i_id;

			/* --- SPA parameters --- */
			thermal_spa_t.t_spa_Tpolicy_info.min_cpu_power[i_id] =
						MINIMUM_CPU_POWERS[i_id];

			thermal_spa_t.t_spa_Tpolicy_info.min_gpu_power[i_id] =
						MINIMUM_GPU_POWERS[i_id];

			tscpu_printk(
				"tscpu_write_atm_setting applied %d %d %d %d %d %d %d %d %d\n",
					i_id,
					FIRST_STEP_TOTAL_POWER_BUDGETS[i_id],
					PACKAGE_THETA_JA_RISES[i_id],
					PACKAGE_THETA_JA_FALLS[i_id],
					MINIMUM_BUDGET_CHANGES[i_id],
					MINIMUM_CPU_POWERS[i_id],
					MAXIMUM_CPU_POWERS[i_id],
					MINIMUM_GPU_POWERS[i_id],
					MAXIMUM_GPU_POWERS[i_id]);
		} else {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT,
					"tscpu_write_atm_setting",
					"Wrong thermal policy");
			#endif
		}

		return count;
	}
	tscpu_dprintk("tscpu_write_atm_setting bad argument\n");
	return -EINVAL;
}

static int tscpu_read_gpu_threshold(struct seq_file *m, void *v)
{
	seq_printf(m, "H %d L %d\n", GPU_L_H_TRIP, GPU_L_L_TRIP);
	return 0;
}

static ssize_t tscpu_write_gpu_threshold
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;

	int gpu_h = -1, gpu_l = -1;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d", &gpu_h, &gpu_l) >= 2) {
		tscpu_printk("%s input %d %d\n", __func__,
								gpu_h, gpu_l);

		if ((gpu_h > 0) && (gpu_l > 0) && (gpu_h > gpu_l)) {
			GPU_L_H_TRIP = gpu_h;
			GPU_L_L_TRIP = gpu_l;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
			atm_update_cg_alloc_param_to_sspm();
#endif
#endif

			tscpu_printk(
				"%s applied %d %d\n", __func__,
				GPU_L_H_TRIP, GPU_L_L_TRIP);
		} else {
			tscpu_dprintk(
				"%s out of range\n", __func__);
		}

		return count;
	}
	tscpu_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}

/* +ASC+ */
static int tscpu_read_atm(struct seq_file *m, void *v)
{

	seq_printf(m, "[%s] ver = %d\n", __func__, tscpu_atm);
	seq_printf(m, "tt_ratio_high_rise = %d\n", tt_ratio_high_rise);
	seq_printf(m, "tt_ratio_high_fall = %d\n", tt_ratio_high_fall);
	seq_printf(m, "tt_ratio_low_rise = %d\n", tt_ratio_low_rise);
	seq_printf(m, "tt_ratio_low_fall = %d\n", tt_ratio_low_fall);
	seq_printf(m, "tp_ratio_high_rise = %d\n", tp_ratio_high_rise);
	seq_printf(m, "tp_ratio_high_fall = %d\n", tp_ratio_high_fall);
	seq_printf(m, "tp_ratio_low_rise = %d\n", tp_ratio_low_rise);
	seq_printf(m, "tp_ratio_low_fall = %d\n", tp_ratio_low_fall);

#ifdef CONFIG_MACH_MT8168
	dump_power_table();
#endif
	return 0;
}

static ssize_t tscpu_write_atm
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	int atm_ver;
	int tmp_tt_ratio_high_rise;
	int tmp_tt_ratio_high_fall;
	int tmp_tt_ratio_low_rise;
	int tmp_tt_ratio_low_fall;
	int tmp_tp_ratio_high_rise;
	int tmp_tp_ratio_high_fall;
	int tmp_tp_ratio_low_rise;
	int tmp_tp_ratio_low_fall;
	int len = 0;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d %d %d %d %d %d %d ",
		   &atm_ver, &tmp_tt_ratio_high_rise, &tmp_tt_ratio_high_fall,
		&tmp_tt_ratio_low_rise, &tmp_tt_ratio_low_fall,
		&tmp_tp_ratio_high_rise, &tmp_tp_ratio_high_fall,
		&tmp_tp_ratio_low_rise, &tmp_tp_ratio_low_fall) == 9)
		/* if (5 <= sscanf(desc, "%d %d %d %d %d", &log_switch, &hot,
		 *	&normal, &low, &lv_offset))
		 */
	{
		tscpu_atm = atm_ver;
		tt_ratio_high_rise = tmp_tt_ratio_high_rise;
		tt_ratio_high_fall = tmp_tt_ratio_high_fall;
		tt_ratio_low_rise = tmp_tt_ratio_low_rise;
		tt_ratio_low_fall = tmp_tt_ratio_low_fall;
		tp_ratio_high_rise = tmp_tp_ratio_high_rise;
		tp_ratio_high_fall = tmp_tp_ratio_high_fall;
		tp_ratio_low_rise = tmp_tp_ratio_low_rise;
		tp_ratio_low_fall = tmp_tp_ratio_low_fall;

#if PRECISE_HYBRID_POWER_BUDGET
		if (tscpu_atm == 3)
			_adaptive_power_calc = _adaptive_power_ppb;
		else
			_adaptive_power_calc = _adaptive_power;
#endif

		return count;
	}
	tscpu_printk("%s bad argument\n", __func__);
	return -EINVAL;

}

/* -ASC- */
#if THERMAL_HEADROOM
static int tscpu_read_thp(struct seq_file *m, void *v)
{
	seq_printf(m, "Tpcb pt coef %d\n", p_Tpcb_correlation);
	seq_printf(m, "Tpcb threshold %d\n", Tpcb_trip_point);
	seq_printf(m, "Tj pt coef %d\n", thp_p_tj_correlation);
	seq_printf(m, "thp tj threshold %d\n", thp_threshold_tj);

	return 0;
}

static ssize_t tscpu_write_thp
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;
	int tpcb_coef = -1, tpcb_trip = -1, thp_coef = -1, thp_threshold = -1;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d %d", &tpcb_coef, &tpcb_trip, &thp_coef,
	&thp_threshold) >= 4) {
		tscpu_printk("%s input %d %d %d %d\n", __func__,
				tpcb_coef, tpcb_trip, thp_coef, thp_threshold);

		p_Tpcb_correlation = tpcb_coef;
		Tpcb_trip_point = tpcb_trip;
		thp_p_tj_correlation = thp_coef;
		thp_threshold_tj = thp_threshold;

		return count;
	}
	tscpu_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}
#endif

#if CONTINUOUS_TM
static int tscpu_read_ctm(struct seq_file *m, void *v)
{
	seq_printf(m, "ctm %d\n", ctm_on);
	seq_printf(m, "Target Tj 0 %d\n", MAX_TARGET_TJ);
	seq_printf(m, "Target Tj 2 %d\n", STEADY_TARGET_TJ);
	seq_printf(m, "Tpcb 1 %d\n", TRIP_TPCB);
	seq_printf(m, "Tpcb 2 %d\n", STEADY_TARGET_TPCB);
	seq_printf(m, "Exit Tj 0 %d\n", MAX_EXIT_TJ);
	seq_printf(m, "Exit Tj 2 %d\n", STEADY_EXIT_TJ);
	seq_printf(m, "Enter_a %d\n", COEF_AE);
	seq_printf(m, "Enter_b %d\n", COEF_BE);
	seq_printf(m, "Exit_a %d\n", COEF_AX);
	seq_printf(m, "Exit_b %d\n", COEF_BX);

	/* +++ cATM+ parameters +++ */
	seq_printf(m, "K_TT %d\n", K_TT);
	seq_printf(m, "MAX_K_SUM_TT %d\n", MAX_K_SUM_TT);
	seq_printf(m, "K_SUM_TT_LOW %d\n", K_SUM_TT_LOW);
	seq_printf(m, "K_SUM_TT_HIGH %d\n", K_SUM_TT_HIGH);
	seq_printf(m, "MIN_SUM_TT %d\n", MIN_SUM_TT);
	seq_printf(m, "MAX_SUM_TT %d\n", MAX_SUM_TT);
	seq_printf(m, "MIN_TTJ %d\n", MIN_TTJ);
	seq_printf(m, "CATMP_STEADY_TTJ_DELTA %d\n", CATMP_STEADY_TTJ_DELTA);
	/* --- cATM+ parameters --- */

#if defined(CATM_TPCB_EXTEND)
	seq_printf(m, "TPCB_EXTEND %d\n", TPCB_EXTEND);
#endif
	return 0;
}

static ssize_t tscpu_write_ctm(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[256];
	int len = 0;
	int t_ctm_on = -1, t_MAX_TARGET_TJ = -1, t_STEADY_TARGET_TJ = -1,
		t_TRIP_TPCB = -1, t_STEADY_TARGET_TPCB = -1, t_MAX_EXIT_TJ = -1,
		t_STEADY_EXIT_TJ = -1, t_COEF_AE = -1, t_COEF_BE = -1,
		t_COEF_AX = -1, t_COEF_BX = -1, t_K_SUM_TT_HIGH = -1,
		t_K_SUM_TT_LOW = -1, t_CATMP_STEADY_TTJ_DELTA = -1,
		t_TPCB_EXTEND = -1;
	int scan_count = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	scan_count =
		sscanf(desc, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		&t_ctm_on, &t_MAX_TARGET_TJ, &t_STEADY_TARGET_TJ, &t_TRIP_TPCB,
		&t_STEADY_TARGET_TPCB, &t_MAX_EXIT_TJ, &t_STEADY_EXIT_TJ,
		&t_COEF_AE, &t_COEF_BE, &t_COEF_AX, &t_COEF_BX,
		&t_K_SUM_TT_HIGH, &t_K_SUM_TT_LOW, &t_CATMP_STEADY_TTJ_DELTA,
		&t_TPCB_EXTEND);

	if (scan_count >= 11) {
		tscpu_printk("%s input %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			__func__, t_ctm_on, t_MAX_TARGET_TJ, t_STEADY_TARGET_TJ,
			t_TRIP_TPCB, t_STEADY_TARGET_TPCB,
			t_MAX_EXIT_TJ, t_STEADY_EXIT_TJ,
			t_COEF_AE, t_COEF_BE, t_COEF_AX, t_COEF_BX,
			t_K_SUM_TT_HIGH, t_K_SUM_TT_LOW,
			t_CATMP_STEADY_TTJ_DELTA,
			t_TPCB_EXTEND);

		if (t_ctm_on < 0 || t_ctm_on > 2) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
							DB_OPT_DEFAULT,
							"tscpu_write_ctm",
							"Wrong thermal policy");
			#endif
		}
		if (t_MAX_TARGET_TJ < -20000 || t_MAX_TARGET_TJ > 200000) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
							DB_OPT_DEFAULT,
							"tscpu_write_ctm",
							"Wrong thermal policy");
			#endif
		}
		if (t_STEADY_TARGET_TJ < -20000
				|| t_STEADY_TARGET_TJ > 200000){
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
							DB_OPT_DEFAULT,
							"tscpu_write_ctm",
							"Wrong thermal policy");
			#endif
		}
		if (t_TRIP_TPCB < -20000 || t_TRIP_TPCB > 200000) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
							DB_OPT_DEFAULT,
							"tscpu_write_ctm",
							"Wrong thermal policy");
			#endif
		}
		if (t_STEADY_TARGET_TPCB < -20000
		|| t_STEADY_TARGET_TPCB > 200000) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
							DB_OPT_DEFAULT,
							"tscpu_write_ctm",
							"Wrong thermal policy");
			#endif
		}
		if (t_MAX_EXIT_TJ < -20000 || t_MAX_EXIT_TJ > 200000) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
							DB_OPT_DEFAULT,
							"tscpu_write_ctm",
							"Wrong thermal policy");
			#endif
		}
		if (t_STEADY_EXIT_TJ < -20000 || t_STEADY_EXIT_TJ > 200000) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
							DB_OPT_DEFAULT,
							"tscpu_write_ctm",
							"Wrong thermal policy");
			#endif
		}
		if (t_COEF_AE < 0 || t_COEF_BE < 0
			|| t_COEF_AX < 0 || t_COEF_BX < 0) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
							DB_OPT_DEFAULT,
							"tscpu_write_ctm",
							"Wrong thermal policy");
			#endif
		}
		/* no parameter checking here */
		ctm_on = t_ctm_on;	/* 2: cATM+, 1: cATMv1, 0: off */

		MAX_TARGET_TJ = t_MAX_TARGET_TJ;
		STEADY_TARGET_TJ = t_STEADY_TARGET_TJ;
		TRIP_TPCB = t_TRIP_TPCB;
		STEADY_TARGET_TPCB = t_STEADY_TARGET_TPCB;
		MAX_EXIT_TJ = t_MAX_EXIT_TJ;
		STEADY_EXIT_TJ = t_STEADY_EXIT_TJ;

		COEF_AE = t_COEF_AE;
		COEF_BE = t_COEF_BE;
		COEF_AX = t_COEF_AX;
		COEF_BX = t_COEF_BX;

#if defined(CATM_TPCB_EXTEND)
		if (g_turbo_bin && (STEADY_TARGET_TPCB >= 52000)) {
			if (t_TPCB_EXTEND > 0 && t_TPCB_EXTEND < 10000) {
				TRIP_TPCB += t_TPCB_EXTEND;
				STEADY_TARGET_TPCB += t_TPCB_EXTEND;
				COEF_AE = STEADY_TARGET_TJ +
					(STEADY_TARGET_TPCB * COEF_BE) / 1000;
				COEF_AX = STEADY_EXIT_TJ +
					(STEADY_TARGET_TPCB * COEF_BX) / 1000;
				TPCB_EXTEND = t_TPCB_EXTEND;
			}
		}
#endif

		/* +++ cATM+ parameters +++ */
		if (ctm_on == 2) {
			if (t_K_SUM_TT_HIGH >= 0
				&& t_K_SUM_TT_HIGH < MAX_K_SUM_TT)
				K_SUM_TT_HIGH = t_K_SUM_TT_HIGH;

			if (t_K_SUM_TT_LOW >= 0
				&& t_K_SUM_TT_LOW < MAX_K_SUM_TT)
				K_SUM_TT_LOW = t_K_SUM_TT_LOW;

			if (t_CATMP_STEADY_TTJ_DELTA >= 0)
				CATMP_STEADY_TTJ_DELTA =
						t_CATMP_STEADY_TTJ_DELTA;

			catmplus_update_params();
		}
		/* --- cATM+ parameters --- */

		/* --- SPA parameters --- */
		thermal_spa_t.t_spa_Tpolicy_info.steady_target_tj =
							STEADY_TARGET_TJ;

		thermal_spa_t.t_spa_Tpolicy_info.steady_exit_tj =
							STEADY_EXIT_TJ;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
		atm_update_catm_param_to_sspm();
#endif
#endif

		return count;
	}

	tscpu_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}
#endif

#if PRECISE_HYBRID_POWER_BUDGET
static int tscpu_read_phpb(struct seq_file *m, void *v)
{
	int i;
	struct phpb_param *p;

	for (i = 0; i < NR_PHPB_PARAMS; i++) {
		p = &phpb_params[i];
		seq_printf(m, "[%s] %d %d\n", p->type, p->tt, p->tp);
	}
	seq_printf(m, "[common] %d\n", phpb_theta_max);

	return 0;
}

static ssize_t tscpu_write_phpb(struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{
	char *buf, *ori_buf;
	int i, tt, tp;
	int __theta;
	int ret = -EINVAL;
	struct phpb_param *p;

	tscpu_printk("%s, input str len = %zu\n", __func__, count);

	if (count >= 128 || count < 1)
		return -EINVAL;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -EFAULT;
	ori_buf = buf;

	if (copy_from_user(buf, buffer, count)) {
		ret = -EFAULT;
		goto exit;
	}

	buf[count] = '\0';

	if (strstr(buf, " ") == NULL)
		goto exit;

	for (i = 0; i < NR_PHPB_PARAMS; i++) {
		p = &phpb_params[i];
		if (strstr(buf, p->type))
			break;
	}

	if (i < NR_PHPB_PARAMS) {
		strsep(&buf, " ");
		if (sscanf(buf, "%d %d", &tt, &tp) != 2)
			goto exit;
		/* TODO verify values */
		p->tt = tt;
		p->tp = tp;
	} else {
		if (strstr(buf, "common") == NULL)
			goto exit;
		strsep(&buf, " ");
		if (kstrtoint(buf, 10, &__theta) != 0)
			goto exit;

		if (__theta < phpb_theta_min)
			goto exit;
		phpb_theta_max = __theta;
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	atm_update_ppb_param_to_sspm();
#endif
#endif

	ret = count;

exit:
	kfree(ori_buf);
	return ret;
}

static void phpb_params_init(void)
{
#if defined(CLATM_SET_INIT_CFG)
	phpb_params[PHPB_PARAM_CPU].tt = CLATM_INIT_CFG_PHPB_CPU_TT;
	phpb_params[PHPB_PARAM_CPU].tp = CLATM_INIT_CFG_PHPB_CPU_TP;
#else
	phpb_params[PHPB_PARAM_CPU].tt = 20;
	phpb_params[PHPB_PARAM_CPU].tp = 20;
#endif
	strncpy(phpb_params[PHPB_PARAM_CPU].type, "cpu", 3);
	phpb_params[PHPB_PARAM_CPU].type[3] = '\0';

#if defined(CLATM_SET_INIT_CFG)
	phpb_params[PHPB_PARAM_GPU].tt = CLATM_INIT_CFG_PHPB_GPU_TT;
	phpb_params[PHPB_PARAM_GPU].tp = CLATM_INIT_CFG_PHPB_GPU_TP;
#else
	phpb_params[PHPB_PARAM_GPU].tt = 80;
	phpb_params[PHPB_PARAM_GPU].tp = 80;
#endif
	strncpy(phpb_params[PHPB_PARAM_GPU].type, "gpu", 3);
	phpb_params[PHPB_PARAM_GPU].type[3] = '\0';
}
#endif	/* PRECISE_HYBRID_POWER_BUDGET */

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
static int atm_sspm_read(struct seq_file *m, void *v)
{
	struct thermal_ipi_data thermal_data;
	static int s_cpu_limit, s_gpu_limit;

	if (atm_sspm_enabled == 1) {
		int cpu_limit, gpu_limit;

		if (atm_to_sspm(THERMAL_IPI_GET_ATM_CPU_LIMIT, 1,
			&thermal_data, &cpu_limit) >= 0)
			s_cpu_limit = cpu_limit;

		if (atm_to_sspm(THERMAL_IPI_GET_ATM_GPU_LIMIT, 1,
			&thermal_data, &gpu_limit) >= 0)
			s_gpu_limit = gpu_limit;
	} else {
		s_cpu_limit = 0;
		s_gpu_limit = 0;
	}
	seq_printf(m, "%d,%d,%d\n", atm_sspm_enabled, s_cpu_limit, s_gpu_limit);
	seq_printf(m, "atm sspm %d\n", atm_sspm_enabled);

	return 0;
}

static ssize_t atm_sspm_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0;
	int t_enabled = -1;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return -EFAULT;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &t_enabled) == 0) {
		tscpu_printk("%s en %d\n", __func__, t_enabled);

		if (t_enabled == 0) {
			atm_enable_atm_in_sspm(0);
			atm_sspm_enabled = 0;
		} else if (t_enabled == 1) {
			int ret = 0;
			int cur_min_gpu_pwr =
				(int)mt_gpufreq_get_min_power() + 1;
			int cur_max_gpu_pwr =
				(int)mt_gpufreq_get_max_power() + 1;

			/* Fix the problem that mMc mMg not updated
			 * when trip point is not reached.
			 */
			MINIMUM_CPU_POWER = MINIMUM_CPU_POWERS[0];
			MAXIMUM_CPU_POWER = MAXIMUM_CPU_POWERS[0];
			MINIMUM_GPU_POWER = MINIMUM_GPU_POWERS[0];
			if (!is_max_gpu_power_specified[0])
				MAXIMUM_GPU_POWERS[0] = cur_max_gpu_pwr;
			else if (MAXIMUM_GPU_POWERS[0] < cur_min_gpu_pwr)
				MAXIMUM_GPU_POWERS[0] = cur_min_gpu_pwr;
			MAXIMUM_GPU_POWER = MAXIMUM_GPU_POWERS[0];

			ret = atm_update_atm_param_to_sspm();
			if (ret == -2) {
				tscpu_printk("%s atm in sspm not supported!\n",
								__func__);

				return count;
			}
			atm_update_ppb_param_to_sspm();
			atm_update_catm_param_to_sspm();
			atm_update_cg_alloc_param_to_sspm();
			atm_update_ttj_to_sspm();
			atm_enable_atm_in_sspm(1);
			atm_sspm_enabled = 1;
			set_adaptive_cpu_power_limit(0);
			set_adaptive_gpu_power_limit(0);
		}

		return count;
	}
	tscpu_printk("%s bad argument\n", __func__);
	return -EINVAL;
}
#endif
#endif

#endif	/* CPT_ADAPTIVE_AP_COOLER */

#if CPT_ADAPTIVE_AP_COOLER
static int tscpu_atm_setting_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_atm_setting, NULL);
}

static const struct file_operations mtktscpu_atm_setting_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_atm_setting_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_atm_setting,
	.release = single_release,
};

#if CLATM_USE_MIN_CPU_OPP
static int tscpu_atm_cpu_min_opp_read(struct seq_file *m, void *v)
{
	int i, j;

	for (i = 0; i < MAX_CPT_ADAPTIVE_COOLERS; i++) {
		seq_printf(m, "%s%02d\n", adaptive_cooler_name, i);
		seq_printf(m, "mode = %d\n", g_c_min_opp.mode[i]);

		seq_printf(m, "min CPU power = %d\n",
			g_c_min_opp.min_CPU_power[i]);

		seq_printf(m, "min CPU power from opp = %d\n",
			g_c_min_opp.min_CPU_power_from_opp[i]);

		seq_printf(m, "current min cpu power = %d\n",
			MINIMUM_CPU_POWERS[i]);

		for (j = 0; j < NR_PPM_CLUSTERS; j++) {
			seq_printf(m, "cluster%02d core %d, freq_idx %d\n",
				j, g_c_min_opp.cpu_opp_set[i][j].core_num,
				g_c_min_opp.cpu_opp_set[i][j].freq_idx);
		}

		seq_puts(m, "\n");
	}

	seq_puts(m, "Two commands\n");
	seq_puts(m, "1. Set a set of min cpu opp\n");
	seq_puts(m, "   echo [ATM_ID] [N_CLUSTER] [CORE_0] [F_IDX0] [CORE_1] [F_IDX1]");
	seq_puts(m, " [CORE_2] [F_IDX2] > /proc/driver/thermal/clatm_cpu_min_opp\n");
	seq_puts(m, "   ATM_ID: 0:cpu_adaptive_00, 1: cpu_adaptive_01, 2: cpu_adaptive_02\n");
	seq_puts(m, "   N_CLUSTER: number of clusters in this platform\n");
	seq_puts(m, "   CORE_0: number of cores in cluster 0\n");
	seq_puts(m, "   F_IDX_0: frequency opp index in cluster 0\n");
	seq_puts(m, "   and etc.\n");
	seq_puts(m, "2. Change mode\n");
	seq_puts(m, "   echo chmod [MODE_ID] > /proc/driver/thermal/clatm_cpu_min_opp\n");
	seq_puts(m, "   MODE_ID:\n");
	seq_puts(m, "      1: Use a conventional min cpu power budget\n");
	seq_puts(m, "      2: Use a set of min cpu opp\n");

	return 0;
}

static ssize_t tscpu_atm_cpu_min_opp_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128], cmd[20];
	int i, len = 0, arg;
	int atm_id, num_cluster, core[3],
		freq_idx[3];

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	if (sscanf(desc, "%d %d %d %d %d %d %d %d", &atm_id, &num_cluster,
		&core[0], &freq_idx[0], &core[1], &freq_idx[1],
		&core[2], &freq_idx[2]) == 8) {
		if (atm_id < 0 || atm_id >= MAX_CPT_ADAPTIVE_COOLERS) {
			tscpu_printk("Bad arg: atm_id error\n");
			goto BAD_ARG;
		}

		if (num_cluster != NR_PPM_CLUSTERS) {
			g_c_min_opp.mode[atm_id] = 0;
			tscpu_printk("Bad arg: Total number of clusters doesn't match\n");
			goto BAD_ARG;
		}

		for (i = 0; i < NR_PPM_CLUSTERS; i++) {
			g_c_min_opp.cpu_opp_set[atm_id][i].core_num = core[i];
			g_c_min_opp.cpu_opp_set[atm_id][i].freq_idx
				= freq_idx[i];
		}

		if (g_c_min_opp.min_CPU_power[atm_id] == 0)
			g_c_min_opp.min_CPU_power[atm_id] =
				MINIMUM_CPU_POWERS[atm_id];

		g_c_min_opp.min_CPU_power_from_opp[atm_id] =
			ppm_find_pwr_idx(g_c_min_opp.cpu_opp_set[atm_id]);

		if (g_c_min_opp.min_CPU_power_from_opp[atm_id] == -1) {
			g_c_min_opp.mode[atm_id] = 0;
			tscpu_printk("Error: When transfer a CPU opp to a power budget\n");
			goto BAD_ARG;
		}

		g_c_min_opp.min_CPU_power_from_opp[atm_id] += 1;
		MINIMUM_CPU_POWERS[atm_id] =
			g_c_min_opp.min_CPU_power_from_opp[atm_id];
		thermal_spa_t.t_spa_Tpolicy_info.min_cpu_power[atm_id] =
			g_c_min_opp.min_CPU_power_from_opp[atm_id];

		g_c_min_opp.mode[atm_id] = 2;

		return count;
	} else if (sscanf(desc, "%19s %d", cmd, &arg) == 2) {
		if ((strncmp(cmd, "chmod", 5) == 0)) {
			if (arg != 1 && arg != 2) {
				tscpu_printk("Bad arg: mode should only be 1 and 2\n");
				goto BAD_ARG;
			}

			for (i = 0; i < MAX_CPT_ADAPTIVE_COOLERS; i++) {
				if (g_c_min_opp.mode[i] == 0) {
					tscpu_printk("Skip cpu_adaptive_%d, because didn't initialized\n",
						i);
					continue;
				}
				if (arg == 1) {
					if (g_c_min_opp.min_CPU_power[i] == 0)
						continue;

					MINIMUM_CPU_POWERS[i] =
						g_c_min_opp.min_CPU_power[i];
			thermal_spa_t.t_spa_Tpolicy_info.min_cpu_power[i] =
					g_c_min_opp.min_CPU_power[i];
				} else if (arg == 2) {
					MINIMUM_CPU_POWERS[i] =
					g_c_min_opp.min_CPU_power_from_opp[i];
			thermal_spa_t.t_spa_Tpolicy_info.min_cpu_power[i] =
					g_c_min_opp.min_CPU_power_from_opp[i];
				}

				g_c_min_opp.mode[i] = arg;
			}
			return count;
		}

		tscpu_printk("Bad arg: No this command\n");
		goto BAD_ARG;
	}
BAD_ARG:
	tscpu_printk("%s,%d: bad argument, %s\n", __func__, __LINE__, desc);
	return -EINVAL;
}

static int tscpu_atm_cpu_min_opp_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_atm_cpu_min_opp_read, NULL);
}

static const struct file_operations mtktscpu_atm_cpu_min_opp_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_atm_cpu_min_opp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_atm_cpu_min_opp_write,
	.release = single_release,
};
#endif

static int tscpu_gpu_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_gpu_threshold, NULL);
}

static const struct file_operations mtktscpu_gpu_threshold_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_gpu_threshold_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_gpu_threshold,
	.release = single_release,
};

/* +ASC+ */
static int tscpu_open_atm(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_atm, NULL);
}

static const struct file_operations mtktscpu_atm_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_open_atm,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_atm,
	.release = single_release,
};
/* -ASC- */

#if THERMAL_HEADROOM
static int tscpu_thp_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_thp, NULL);
}

static const struct file_operations mtktscpu_thp_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_thp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_thp,
	.release = single_release,
};
#endif

#if CONTINUOUS_TM
static int tscpu_ctm_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_ctm, NULL);
}

static const struct file_operations mtktscpu_ctm_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_ctm_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_ctm,
	.release = single_release,
};
#endif				/* CONTINUOUS_TM */

#if PRECISE_HYBRID_POWER_BUDGET
static int tscpu_phpb_open(struct inode *inode, struct file *file)
{
	return single_open(file, tscpu_read_phpb, NULL);
}

static const struct file_operations mtktscpu_phpb_fops = {
	.owner = THIS_MODULE,
	.open = tscpu_phpb_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = tscpu_write_phpb,
	.release = single_release,
};
#endif

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
static int atm_sspm_open(struct inode *inode, struct file *file)
{
	return single_open(file, atm_sspm_read, NULL);
}

static const struct file_operations atm_sspm_fops = {
	.owner = THIS_MODULE,
	.open = atm_sspm_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = atm_sspm_write,
	.release = single_release,
};
#endif
#endif

#endif				/* CPT_ADAPTIVE_AP_COOLER */

#if PRECISE_HYBRID_POWER_BUDGET
static void phpb_init(struct proc_dir_entry *mtktscpu_dir)
{
	struct proc_dir_entry *entry;

	phpb_params_init();

	entry = proc_create("clphpb", 0664,
					mtktscpu_dir, &mtktscpu_phpb_fops);

	if (entry)
		proc_set_user(entry, uid, gid);
}
#endif

static void tscpu_cooler_create_fs(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktscpu_dir = NULL;

	mtktscpu_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscpu_dir) {
		tscpu_printk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);

	} else {
#if CPT_ADAPTIVE_AP_COOLER
		entry = proc_create("clatm_setting",
				0664,
				mtktscpu_dir, &mtktscpu_atm_setting_fops);

		if (entry)
			proc_set_user(entry, uid, gid);

#if CLATM_USE_MIN_CPU_OPP
		entry = proc_create("clatm_cpu_min_opp",
				0664,
				mtktscpu_dir, &mtktscpu_atm_cpu_min_opp_fops);

		if (entry)
			proc_set_user(entry, uid, gid);
#endif

		entry = proc_create("clatm_gpu_threshold",
				0664,
				mtktscpu_dir, &mtktscpu_gpu_threshold_fops);

		if (entry)
			proc_set_user(entry, uid, gid);
#endif				/* #if CPT_ADAPTIVE_AP_COOLER */


		/* +ASC+ */
		entry = proc_create("clatm",
				0664,
				mtktscpu_dir, &mtktscpu_atm_fops);

		if (entry)
			proc_set_user(entry, uid, gid);
		/* -ASC- */

#if THERMAL_HEADROOM
		entry = proc_create("clthp",
				0664,
				mtktscpu_dir, &mtktscpu_thp_fops);

		if (entry)
			proc_set_user(entry, uid, gid);
#endif

#if CONTINUOUS_TM
		entry = proc_create("clctm",
				0664,
				mtktscpu_dir, &mtktscpu_ctm_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
#endif

#if PRECISE_HYBRID_POWER_BUDGET
		phpb_init(mtktscpu_dir);
#endif

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
		entry = proc_create("clatm_sspm",
				0664,
				mtktscpu_dir, &atm_sspm_fops);

		if (entry)
			proc_set_user(entry, uid, gid);
#endif
#endif

	}
}

#ifdef FAST_RESPONSE_ATM

#if KRTATM_TIMER == KRTATM_HR
void atm_cancel_hrtimer(void)
{
	hrtimer_try_to_cancel(&atm_hrtimer);
}

void atm_restart_hrtimer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, atm_hrtimer_polling_delay);
	hrtimer_start(&atm_hrtimer, ktime, HRTIMER_MODE_REL);
#ifdef ATM_CFG_PROFILING
	atm_resumed = 1;
#endif
}

static unsigned long atm_get_timeout_time(int curr_temp)
{

#ifdef ATM_CFG_PROFILING
	return atm_hrtimer_polling_delay;
#else
	/*
	 * curr_temp can't smaller than -30'C
	 */
	curr_temp = (curr_temp < -30000) ? -30000 : curr_temp;


	if (curr_temp >= 65000)
		return atm_hrtimer_polling_delay;
	else
		return (atm_hrtimer_polling_delay
					<< ((81394 - curr_temp) >> 14));
#endif
}

#elif KRTATM_TIMER == KRTATM_NORMAL

void atm_cancel_hrtimer(void)
{
}

void atm_restart_hrtimer(void)
{
}

static unsigned long atm_get_timeout_time(int curr_temp)
{

#ifdef ATM_CFG_PROFILING
	return atm_timer_polling_delay;
#else

	if (curr_temp >= polling_trip_temp0)
		return atm_timer_polling_delay / polling_factor0;
	else if (curr_temp >= polling_trip_temp1)
		return atm_timer_polling_delay;
	else if (curr_temp >= polling_trip_temp2)
		return atm_timer_polling_delay * polling_factor1;
	else
		return atm_timer_polling_delay * polling_factor2;
#endif
}
#endif


#if KRTATM_TIMER == KRTATM_HR
static enum hrtimer_restart atm_loop(struct hrtimer *timer)
{
	ktime_t ktime;
#elif KRTATM_TIMER == KRTATM_NORMAL
static void atm_loop(unsigned long data)
{
#endif
	int temp;
#ifdef ENALBE_UART_LIMIT
#if ENALBE_UART_LIMIT
	static int hasDisabled;
#endif
#endif
	char buffer[128];
	unsigned long polling_time;
#if KRTATM_TIMER == KRTATM_HR
	unsigned long polling_time_s;
	unsigned long polling_time_ns;
#endif
	ktime_t now;

	now = ktime_get();

	tscpu_workqueue_start_timer();

	atm_prev_maxtj = atm_curr_maxtj;
	atm_curr_maxtj = tscpu_get_curr_temp();


	atm_prev_maxtj_time = atm_curr_maxtj_time;
	atm_curr_maxtj_time = ktime_to_us(now);

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
	if (atm_sspm_enabled == 1) {
#ifdef ATM_CFG_PROFILING
		atm_resumed = 1; /* Must skip last timestamp. */
#endif
		goto exit;
	}
#endif
#endif

	temp = sprintf(buffer, "%s c %d p %d l %d ct %lld pt %lld ", __func__,
						atm_curr_maxtj, atm_prev_maxtj,
						adaptive_cpu_power_limit,
						atm_curr_maxtj_time,
						atm_prev_maxtj_time);
#if 0
	if (atm_curr_maxtj >= 100000
		|| (atm_curr_maxtj - atm_prev_maxtj >= 15000))
		print_risky_temps(buffer, temp, 1);
	else
		print_risky_temps(buffer, temp, 0);
#endif

#ifdef ENALBE_UART_LIMIT
#if ENALBE_UART_LIMIT
	temp = atm_curr_maxtj;
	if ((TEMP_DIS_UART - TEMP_TOLERANCE) < temp) {
		/************************************************
		 *	Disable UART log
		 ************************************************
		 */
		if (mt_get_uartlog_status()) {
			hasDisabled = 1;
			set_uartlog_status(false);
		}
	}

	if (temp < (TEMP_EN_UART + TEMP_TOLERANCE)) {
		/*************************************************
		 *	Restore UART log
		 ************************************************
		 */
		if (!mt_get_uartlog_status() && hasDisabled)
			set_uartlog_status(true);

		hasDisabled = 0;
	}
#endif
#endif

	if (krtatm_thread_handle != NULL)
		wake_up_process(krtatm_thread_handle);


#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#if THERMAL_ENABLE_TINYSYS_SSPM && CPT_ADAPTIVE_AP_COOLER &&	\
	PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
exit:
#endif
#endif

	polling_time = atm_get_timeout_time(atm_curr_maxtj);

#if defined(THERMAL_APU_UNLIMIT)
	if (cl_get_apu_status() == 1) {/*flag not be cleared*/
		total_apu_polling_time =
			total_apu_polling_time + polling_time;
	} else {
		/*flag is cleared, clear timer*/
		total_apu_polling_time = 0;
	}

	/*count till 10 sec(10000ms) timeout*/
	if (total_apu_polling_time >= 10000) {
		total_apu_polling_time = 0;
		cl_set_apu_status(0);//clear apu flag

		tscpu_printk("%s ainr: total_apu_polling_time = 0\n",
			__func__);
	}
#endif

#if KRTATM_TIMER == KRTATM_HR

	/*avoid overflow*/
	if (polling_time > (1000000000-1)) {
		polling_time_s = polling_time / 1000000000;
		polling_time_ns = polling_time % 1000000000;
		ktime = ktime_set(polling_time_s, polling_time_ns);
		/* tscpu_warn("%s polling_time_s=%ld  "
		 *	"polling_time_ns=%ld\n", __func__,
		 *	polling_time_s,polling_time_ns);
		 */

	} else {
		ktime = ktime_set(0, polling_time);
	}

	hrtimer_forward_now(timer, ktime);

	return HRTIMER_RESTART;
#elif KRTATM_TIMER == KRTATM_NORMAL

	atm_timer.expires = jiffies + msecs_to_jiffies(polling_time);
	add_timer(&atm_timer);

#endif

}

#if KRTATM_TIMER == KRTATM_HR
static void atm_hrtimer_init(void)
{
	ktime_t ktime;

	tscpu_dprintk("%s\n", __func__);

	/*100000000 = 100 ms,polling delay can't larger than 100ms*/
	atm_hrtimer_polling_delay =
			(atm_hrtimer_polling_delay < 100000000) ?
		atm_hrtimer_polling_delay : 100000000;

	ktime = ktime_set(0, atm_hrtimer_polling_delay);

	hrtimer_init(&atm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	atm_hrtimer.function = atm_loop;
	hrtimer_start(&atm_hrtimer, ktime, HRTIMER_MODE_REL);
}
#elif KRTATM_TIMER == KRTATM_NORMAL
static void atm_timer_init(void)
{
	tscpu_dprintk("%s\n", __func__);

	/*polling delay can't larger than 100ms*/
	atm_timer_polling_delay = (atm_timer_polling_delay < 100) ?
		atm_timer_polling_delay : 100;

	init_timer_deferrable(&atm_timer);
	atm_timer.function = &atm_loop;
	atm_timer.data = (unsigned long)&atm_timer;
	atm_timer.expires =
		jiffies + msecs_to_jiffies(atm_timer_polling_delay);

	add_timer(&atm_timer);
}
#endif

#define KRTATM_RT	(1)
#define KRTATM_CFS	(2)
#define KRTATM_SCH	KRTATM_CFS

static int krtatm_thread(void *arg)
{
#ifdef ATM_CFG_PROFILING
	ktime_t last, delta;
#endif

#if KRTATM_SCH == KRTATM_RT
	struct sched_param param = {.sched_priority = 98 };

	sched_setscheduler(current, SCHED_FIFO, &param);
#elif KRTATM_SCH == KRTATM_CFS
	set_user_nice(current, MIN_NICE);
#endif
	set_current_state(TASK_INTERRUPTIBLE);

	tscpu_dprintk("%s 1st run\n", __func__);

	schedule();

	for (;;) {
#ifdef ATM_CFG_PROFILING
		if (atm_resumed) {
			atm_resumed = 0;
		} else {
			delta = ktime_get();
			if (ktime_after(delta, last))
				atm_profile_atm_period(
				ktime_to_us(ktime_sub(delta, last)));
		}
		last = ktime_get();
#endif
		tscpu_dprintk("%s awake\n", __func__);
#if (CONFIG_THERMAL_AEE_RR_REC == 1)
		aee_rr_rec_thermal_ATM_status(ATM_WAKEUP);
#endif
		if (kthread_should_stop())
			break;

		{
#ifdef ATM_CFG_PROFILING
			ktime_t start, end;
#endif
			unsigned int gpu_loading;

#ifdef ATM_CFG_PROFILING
			start = ktime_get();
			cpu_pwr_lmt_latest_delay = 0;
			gpu_pwr_lmt_latest_delay = 0;
#endif

			if (!mtk_get_gpu_loading(&gpu_loading))
				gpu_loading = 0;

			/* use separate prev/curr in krtatm because
			 * krtatm may be blocked by PPM
			 */
			krtatm_prev_maxtj = krtatm_curr_maxtj;
			krtatm_curr_maxtj = atm_curr_maxtj;
			if (krtatm_prev_maxtj == 0)
				krtatm_prev_maxtj = atm_prev_maxtj;

			_adaptive_power_calc(krtatm_prev_maxtj,
						krtatm_curr_maxtj,
						(unsigned int) gpu_loading);

			/* To confirm if krtatm kthread is really running. */
			if (krtatm_curr_maxtj >= 100000 ||
			(krtatm_curr_maxtj - krtatm_prev_maxtj >= 20000)) {
				tscpu_warn("%s c %d p %d cl %d gl %d s %d\n",
				__func__, krtatm_curr_maxtj,
				krtatm_prev_maxtj,
				adaptive_cpu_power_limit,
				adaptive_gpu_power_limit,
				cl_dev_adp_cpu_state_active);
				/* dump more info when atm is deactivated */
				if (!cl_dev_adp_cpu_state_active) {
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
					pr_info_ratelimited(TSCPU_LOG_TAG
					"tjs %d ttj %d %d on %d sspm %d %d\n",
					TARGET_TJS[0], TARGET_TJ,
					current_ETJ, ctm_on,
#ifdef THERMAL_SSPM_THERMAL_THROTTLE_SWITCH
					tscpu_sspm_thermal_throttle,
#else
					1, /* disabled */
#endif
#if THERMAL_ENABLE_TINYSYS_SSPM &&	\
	CPT_ADAPTIVE_AP_COOLER && PRECISE_HYBRID_POWER_BUDGET && CONTINUOUS_TM
					atm_sspm_enabled);
#else
					0);
#endif
#else /* !CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
					pr_info_ratelimited(TSCPU_LOG_TAG
					"tjs %d ttj %d %d on %d\n",
					TARGET_TJS[0], TARGET_TJ,
					current_ETJ, ctm_on);
#endif
				}
			}

#ifdef ATM_CFG_PROFILING
			end = ktime_get();
			if (ktime_after(end, start))
				atm_profile_atm_exec((ktime_to_us(
					ktime_sub(end, start)) -
					cpu_pwr_lmt_latest_delay
					- gpu_pwr_lmt_latest_delay));
#endif
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	tscpu_warn("%s stopped\n", __func__);
	return 0;
}
#endif	/* FAST_RESPONSE_ATM */

static int __init mtk_cooler_atm_init(void)
{
	int err = 0;

	tscpu_dprintk("%s: start\n", __func__);

	err = apthermolmt_register_user(&ap_atm, ap_atm_log);
	if (err < 0)
		return err;

#if CPT_ADAPTIVE_AP_COOLER
	/* default use old version */
	_adaptive_power_calc = _adaptive_power;
#if PRECISE_HYBRID_POWER_BUDGET
	if (tscpu_atm == 3)
		_adaptive_power_calc = _adaptive_power_ppb;
#endif
	cl_dev_adp_cpu[0] = mtk_thermal_cooling_device_register(
						"cpu_adaptive_0", NULL,
						&mtktscpu_cooler_adp_cpu_ops);

	cl_dev_adp_cpu[1] = mtk_thermal_cooling_device_register(
						"cpu_adaptive_1", NULL,
						&mtktscpu_cooler_adp_cpu_ops);

	cl_dev_adp_cpu[2] = mtk_thermal_cooling_device_register(
						"cpu_adaptive_2", NULL,
						&mtktscpu_cooler_adp_cpu_ops);

#if defined(CLATM_SET_INIT_CFG)
	mtk_thermal_cooling_device_add_exit_point(
				cl_dev_adp_cpu[0], CLATM_INIT_CFG_0_EXIT_POINT);

	mtk_thermal_cooling_device_add_exit_point(
				cl_dev_adp_cpu[1], CLATM_INIT_CFG_1_EXIT_POINT);

	mtk_thermal_cooling_device_add_exit_point(
				cl_dev_adp_cpu[2], CLATM_INIT_CFG_2_EXIT_POINT);
#endif

#endif
	if (err) {
		tscpu_printk("%s fail\n", __func__);
		return err;
	}
	tscpu_cooler_create_fs();

#ifdef FAST_RESPONSE_ATM

#if KRTATM_TIMER == KRTATM_HR
	atm_hrtimer_init();
#elif KRTATM_SCH == KRTATM_NORMAL
	atm_timer_init();
#endif

#if defined(CATM_TPCB_EXTEND)
	mtk_thermal_get_turbo();
#endif

	tscpu_dprintk("%s creates krtatm\n", __func__);
	krtatm_thread_handle = kthread_create(krtatm_thread,
						(void *)NULL, "krtatm");

	if (IS_ERR(krtatm_thread_handle)) {
		krtatm_thread_handle = NULL;
		tscpu_printk("%s krtatm creation fails\n", __func__);
	} else
		wake_up_process(krtatm_thread_handle);
#endif
#if 0
	reset_gpu_power_history();
#endif
	tscpu_dprintk("%s: end\n", __func__);
	return 0;
}

static void __exit mtk_cooler_atm_exit(void)
{
#ifdef FAST_RESPONSE_ATM

#if KRTATM_TIMER == KRTATM_HR
	hrtimer_cancel(&atm_hrtimer);
#elif KRTATM_SCH == KRTATM_NORMAL
	del_timer(&atm_timer);
#endif

	if (krtatm_thread_handle)
		kthread_stop(krtatm_thread_handle);
#endif

#if CPT_ADAPTIVE_AP_COOLER
	if (cl_dev_adp_cpu[0]) {
		mtk_thermal_cooling_device_unregister(
						cl_dev_adp_cpu[0]);

		cl_dev_adp_cpu[0] = NULL;
	}

	if (cl_dev_adp_cpu[1]) {
		mtk_thermal_cooling_device_unregister(
						cl_dev_adp_cpu[1]);

		cl_dev_adp_cpu[1] = NULL;
	}

	if (cl_dev_adp_cpu[2]) {
		mtk_thermal_cooling_device_unregister(
						cl_dev_adp_cpu[2]);

		cl_dev_adp_cpu[2] = NULL;
	}
#endif

	apthermolmt_unregister_user(&ap_atm);
}

module_init(mtk_cooler_atm_init);
module_exit(mtk_cooler_atm_exit);
