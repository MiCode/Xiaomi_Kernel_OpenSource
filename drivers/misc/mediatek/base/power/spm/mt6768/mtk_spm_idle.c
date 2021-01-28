/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>

#include <mt-plat/mtk_ccci_common.h> /* exec_ccci_kern_func_by_md_id */
#include <mt-plat/mtk_wd_api.h> /* ap wdt related definitons */

#include <trace/events/mtk_idle_event.h>

#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <mtk_spm_suspend_internal.h>
#include <mtk_spm_resource_req.h>

#include "mtk_spm_internal.h"
#include "pwr_ctrl.h"


#define MTK_IDLE_GS_DUMP_READY	(1)

#if defined(MTK_IDLE_GS_DUMP_READY)
/* NOTE: Check golden setting dump header file for each project */
#include "power_gs_v1/mtk_power_gs_internal.h"
#endif

/* FIXME: IT with vcorefs ? */
void __attribute__((weak)) dvfsrc_md_scenario_update(bool suspend) {}

/********************************************************************
 * dp/so3/so pcm_flags and pcm_flags1
 *******************************************************************/

static unsigned int idle_pcm_flags[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] =
		/* SPM_FLAG_DIS_CPU_PDN | */
		SPM_FLAG_DIS_INFRA_PDN |
		/* SPM_FLAG_DIS_DDRPHY_PDN | */
		SPM_FLAG_DIS_VCORE_DVS |
		SPM_FLAG_DIS_VCORE_DFS |
		SPM_FLAG_DIS_ATF_ABORT |
		SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
		#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
		SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
		#endif
		SPM_FLAG_DEEPIDLE_OPTION,

	[IDLE_TYPE_SO3] =
		SPM_FLAG_DIS_INFRA_PDN |
		SPM_FLAG_DIS_VCORE_DVS |
		SPM_FLAG_DIS_VCORE_DFS |
		SPM_FLAG_DIS_ATF_ABORT |
		SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
		#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
		SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
		#endif
		SPM_FLAG_SODI_OPTION |
		SPM_FLAG_ENABLE_SODI3,

	[IDLE_TYPE_SO] =
		SPM_FLAG_DIS_INFRA_PDN |
		SPM_FLAG_DIS_VCORE_DVS |
		SPM_FLAG_DIS_VCORE_DFS |
		SPM_FLAG_DIS_ATF_ABORT |
		SPM_FLAG_KEEP_CSYSPWRUPACK_HIGH |
		#if !defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
		SPM_FLAG_DIS_SSPM_SRAM_SLEEP |
		#endif
		SPM_FLAG_SODI_OPTION,

	[IDLE_TYPE_RG] = 0,
};

static unsigned int idle_pcm_flags1[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] =
		SPM_FLAG1_ENABLE_CPU_SLEEP_VOLT,

	[IDLE_TYPE_SO3] = 0,

	[IDLE_TYPE_SO] = 0,

	[IDLE_TYPE_RG] = 0,
};


/********************************************************************
 * dp/so3/so pwrctrl variables
 *******************************************************************/
#define INVALID_IDLE_TYPE(type) \
	(type != IDLE_TYPE_DP && type != IDLE_TYPE_SO && \
		type != IDLE_TYPE_SO3)

struct pwr_ctrl pwrctrl_dp;
struct pwr_ctrl pwrctrl_so3;
struct pwr_ctrl pwrctrl_so;

static struct pwr_ctrl *get_pwrctrl[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] = &pwrctrl_dp,
	[IDLE_TYPE_SO3] = &pwrctrl_so3,
	[IDLE_TYPE_SO] = &pwrctrl_so,
};

static void mtk_idle_gs_dump(int idle_type)
{
	#if defined(MTK_IDLE_GS_DUMP_READY)
	if (idle_type == IDLE_TYPE_DP)
		mt_power_gs_dump_dpidle(GS_ALL);
	else if (idle_type == IDLE_TYPE_SO3 || idle_type == IDLE_TYPE_SO)
		mt_power_gs_dump_sodi3(GS_ALL);
	#endif
}

/********************************************************************
 * dp/so3/so trigger wfi
 *******************************************************************/

static void print_ftrace_tag(int idle_type, int cpu, int enter)
{
#if MTK_IDLE_TRACE_TAG_ENABLE
	switch (idle_type) {
	case IDLE_TYPE_DP:
		trace_dpidle_rcuidle(cpu, enter);
		break;
	case IDLE_TYPE_SO:
		trace_sodi_rcuidle(cpu, enter);
		break;
	case IDLE_TYPE_SO3:
		trace_sodi3_rcuidle(cpu, enter);
		break;
	default:
		break;
	}
#endif
}

int mtk_idle_trigger_wfi(int idle_type, unsigned int idle_flag, int cpu)
{
	int spm_dormant_sta = 0;
	struct pwr_ctrl *pwrctrl;
	unsigned int cpuidle_mode[NR_IDLE_TYPES] = {
		[IDLE_TYPE_DP] = MTK_DPIDLE_MODE,
		[IDLE_TYPE_SO3] = MTK_SODI3_MODE,
		[IDLE_TYPE_SO] = MTK_SODI_MODE,
	};
	unsigned int enter_flag[NR_IDLE_TYPES] = {
		[IDLE_TYPE_DP] = SPM_ARGS_DPIDLE,
		[IDLE_TYPE_SO3] = SPM_ARGS_SODI,
		[IDLE_TYPE_SO] = SPM_ARGS_SODI,
	};
	unsigned int leave_flag[NR_IDLE_TYPES] = {
		[IDLE_TYPE_DP] = SPM_ARGS_DPIDLE_FINISH,
		[IDLE_TYPE_SO3] = SPM_ARGS_SODI_FINISH,
		[IDLE_TYPE_SO] = SPM_ARGS_SODI_FINISH,
	};

	if (INVALID_IDLE_TYPE(idle_type))
		return 0;

	/* Dump low power golden setting */
	if (idle_flag & MTK_IDLE_LOG_DUMP_LP_GS)
		mtk_idle_gs_dump(idle_type);

	pwrctrl = get_pwrctrl[idle_type];

	print_ftrace_tag(idle_type, cpu, 1);

	if (is_cpu_pdn(pwrctrl->pcm_flags))
		spm_dormant_sta = mtk_enter_idle_state(cpuidle_mode[idle_type]);
	else {
		mt_secure_call(MTK_SIP_KERNEL_SPM_ARGS
					, enter_flag[idle_type], 0, 0, 0);
		mt_secure_call(MTK_SIP_KERNEL_SPM_LEGACY_SLEEP
					, 0, 0, 0, 0);
		mt_secure_call(MTK_SIP_KERNEL_SPM_ARGS
					, leave_flag[idle_type], 0, 0, 0);
	}

	print_ftrace_tag(idle_type, cpu, 0);

	if (spm_dormant_sta < 0)
		pr_info("mtk_enter_idle_state(%d) ret %d\n",
			cpuidle_mode[idle_type], spm_dormant_sta);

	return spm_dormant_sta;
}


/********************************************************************
 * dp/so3/so setup/cleanup pcm
 *******************************************************************/

static int smc_id[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] = MTK_SIP_KERNEL_SPM_DPIDLE_ARGS,
	[IDLE_TYPE_SO3] = MTK_SIP_KERNEL_SPM_SODI_ARGS,
	[IDLE_TYPE_SO] = MTK_SIP_KERNEL_SPM_SODI_ARGS,
};

static void spm_idle_pcm_setup_before_wfi(
	int idle_type, struct pwr_ctrl *pwrctrl, unsigned int op_cond)
{
	unsigned int resource_usage = 0;

	resource_usage = (op_cond & MTK_IDLE_OPT_SLEEP_DPIDLE) ?
		spm_get_resource_usage_by_user(SPM_RESOURCE_USER_SCP)
		: spm_get_resource_usage();

	if (INVALID_IDLE_TYPE(idle_type))
		return;

	mt_secure_call(smc_id[idle_type], pwrctrl->pcm_flags,
		pwrctrl->pcm_flags1, resource_usage, 0);

	/* [sodi3]
	 * ap wdt works fine without f26m and no need to enable pcm wdt
	 *
	 */
	if (idle_type == IDLE_TYPE_SO3)
		mt_secure_call(MTK_SIP_KERNEL_SPM_PWR_CTRL_ARGS
			, SPM_PWR_CTRL_SODI3, PW_WDT_DISABLE, 1, 0);

	/* [sleep dpidle] bypass wake_src and pcm timer value */
	if (op_cond & MTK_IDLE_OPT_SLEEP_DPIDLE)
		mt_secure_call(MTK_SIP_KERNEL_SPM_SLEEP_DPIDLE_ARGS
			, pwrctrl->timer_val, pwrctrl->wake_src, 0, 0);
}

static void spm_idle_pcm_setup_after_wfi(
	int idle_type, struct pwr_ctrl *pwrctrl, unsigned int op_cond)
{
	if (INVALID_IDLE_TYPE(idle_type))
		return;
}


/********************************************************************
 * dp/so3/so main flow by chip
 *******************************************************************/

static unsigned int slp_dp_timer_val;
static unsigned int slp_dp_wake_src;
static unsigned int slp_dp_last_wr = WR_NONE;
static unsigned long flags;
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
static struct wd_api *wd_api;
static int wd_ret;
#endif

void mtk_idle_pre_process_by_chip(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag)
{
	struct pwr_ctrl *pwrctrl;
	unsigned int pcm_flags;
	unsigned int pcm_flags1;

	if (INVALID_IDLE_TYPE(idle_type))
		return;

	pwrctrl = get_pwrctrl[idle_type];

	/* get pcm_flags and update if needed */
	pcm_flags = idle_pcm_flags[idle_type];
	pcm_flags1 = idle_pcm_flags1[idle_type];

	/* [sleep dpidle only] */
	if (op_cond & MTK_IDLE_OPT_SLEEP_DPIDLE) {

		/* backup timer_val and wake_src */
		slp_dp_timer_val = pwrctrl->timer_val;
		slp_dp_wake_src = pwrctrl->wake_src;

		/* setup sleep wake_src and timer_val */
		pwrctrl->timer_val =
			32768 * __spm_get_wake_period(-1, slp_dp_last_wr);

		/* Get sleep wake_src for sleep dpidle */
		pwrctrl->wake_src = spm_get_sleep_wakesrc();

		/* pre watch dog config */
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)

		wd_ret = get_wd_api(&wd_api);
		if (!wd_ret && wd_api) {
			wd_api->wd_spmwdt_mode_config(WD_REQ_EN
							, WD_REQ_RST_MODE);
			wd_api->wd_suspend_notify();
		} else {
			aee_sram_printk("FAILED TO GET WD API\n");
			pr_info("[IDLE] FAILED TO GET WD API\n");
		}
#endif
	}

	/* initialize pcm_flags/pcm_flags1 */
	__spm_set_pwrctrl_pcm_flags(pwrctrl, pcm_flags);
	__spm_set_pwrctrl_pcm_flags1(pwrctrl, pcm_flags1);

	/* lock spm spin_lock */
	spin_lock_irqsave(&__spm_lock, flags);

	/* mask irq and backup cirq */
	mtk_spm_irq_backup();

	__spm_sync_pcm_flags(pwrctrl);

	/* setup vcore dvfs before idle scenario */
	dvfsrc_md_scenario_update(1);

	/* setup spm */
	spm_idle_pcm_setup_before_wfi(idle_type, pwrctrl, op_cond);
}

static unsigned int mtk_dpidle_output_log(
	int idle_type, const struct wake_status *wakesta,
	unsigned int op_cond, unsigned int idle_flag);

static unsigned int mtk_sodi_output_log(
	int idle_type, const struct wake_status *wakesta,
	unsigned int op_cond, unsigned int idle_flag);

typedef unsigned int (*mtk_idle_log_t) (
	int idle_type, const struct wake_status *wakesta,
	unsigned int op_cond, unsigned int idle_flag);

static mtk_idle_log_t mtk_idle_log[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] = mtk_dpidle_output_log,
	[IDLE_TYPE_SO3] = mtk_sodi_output_log,
	[IDLE_TYPE_SO] = mtk_sodi_output_log,
};

void mtk_idle_post_process_by_chip(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag)
{
	struct pwr_ctrl *pwrctrl;
	struct wake_status wakesta;
	unsigned int wr = WR_NONE;

	if (INVALID_IDLE_TYPE(idle_type))
		return;

	pwrctrl = get_pwrctrl[idle_type];

	/* get spm info */
	__spm_get_wakeup_status(&wakesta);

	/* clean up spm */
	spm_idle_pcm_setup_after_wfi(idle_type, pwrctrl, op_cond);

	/* setup vcore dvfs after idle scenario */
	dvfsrc_md_scenario_update(0);

	/* print log */
	wr = mtk_idle_log[idle_type](idle_type, &wakesta, op_cond, idle_flag);

	/* unmask irq and restore cirq */
	mtk_spm_irq_restore();

	/* unlock spm spin_lock */
	spin_unlock_irqrestore(&__spm_lock, flags);

	/* [sleep dpidle only] */
	if (op_cond & MTK_IDLE_OPT_SLEEP_DPIDLE) {
		/* post watch dog config */
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
		if (!wd_ret) {
			if (!pwrctrl->wdt_disable)
				wd_api->wd_resume_notify();
			else {
				aee_sram_printk(
					"pwrctrl->wdt_disable %d\n",
						pwrctrl->wdt_disable);
				pr_info(
					"[SPM] pwrctrl->wdt_disable %d\n",
						pwrctrl->wdt_disable);
			}

			wd_api->wd_spmwdt_mode_config(WD_REQ_DIS
							, WD_REQ_RST_MODE);
		}
#endif

		/* restore timer_val and wake_src */
		pwrctrl->timer_val = slp_dp_timer_val;
		pwrctrl->wake_src = slp_dp_wake_src;

		/* backup wakeup reason */
		slp_dp_last_wr = wr;
	}
}

unsigned int get_slp_dp_last_wr(void)
{
	return slp_dp_last_wr;
}
/********************************************************************
 * mtk idle output log
 *******************************************************************/
#define IDLE_TIMER_OUT_CRITERIA (32)    /* 1 ms (32k/sec)*/
#define IDLE_PRINT_LOG_DURATION (5000)  /* 5 seconds */

static bool check_print_log_duration(void)
{
	static unsigned long int pre_time;
	unsigned long int cur_time;
	bool ret = false;

	cur_time = spm_get_current_time_ms();
	ret = (cur_time - pre_time > IDLE_PRINT_LOG_DURATION);

	if (ret)
		pre_time = cur_time;

	return ret;
}

/* Print output log - Deep Idle ------------------------ */

static unsigned int mtk_dpidle_output_log(
	int idle_type, const struct wake_status *wakesta,
	unsigned int op_cond, unsigned int idle_flag)
{
	bool print_log = false;
	unsigned int wr = WR_NONE;

	/* No log for latency profiling case */
	if (idle_flag & MTK_IDLE_LOG_DISABLE)
		return WR_NONE;

	/* [sleep dpidle] directly print */
	if (op_cond & MTK_IDLE_OPT_SLEEP_DPIDLE)
		return __spm_output_wake_reason(wakesta, true, "sleep_dpidle");

	if (!(idle_flag & MTK_IDLE_LOG_REDUCE)) {
		print_log = true;
	} else {
		if (wakesta->assert_pc != 0 || wakesta->r12 == 0)
			print_log = true;
		else if (wakesta->timer_out <= IDLE_TIMER_OUT_CRITERIA)
			print_log = true;
		else if (check_print_log_duration())
			print_log = true;
	}

	if (print_log) {
		pr_info("Power/swap op_cond = 0x%x\n", op_cond);
		wr = __spm_output_wake_reason(
			wakesta, false, mtk_idle_name(idle_type));
		if (idle_flag & MTK_IDLE_LOG_RESOURCE_USAGE)
			spm_resource_req_dump();
	}

	return wr;
}

/* Print output log - SODI3 / SODI --------------------- */

static unsigned int mtk_sodi_output_log(
	int idle_type, const struct wake_status *wakesta,
	unsigned int op_cond, unsigned int idle_flag)
{
	bool print_log = false;
	unsigned int wr = WR_NONE;

	/* No log for latency profiling case */
	if (idle_flag & MTK_IDLE_LOG_DISABLE)
		return WR_NONE;

	if (!(idle_flag & MTK_IDLE_LOG_REDUCE)) {
		print_log = true;
	} else {
		if (wakesta->assert_pc != 0 || wakesta->r12 == 0)
			print_log = true;
		else if (wakesta->timer_out <= IDLE_TIMER_OUT_CRITERIA)
			print_log = true;
		else if (check_print_log_duration())
			print_log = true;
	}

	if (print_log) {
		pr_info("Power/swap op_cond = 0x%x\n", op_cond);
		wr = __spm_output_wake_reason(
			wakesta, false, mtk_idle_name(idle_type));
		if (idle_flag & MTK_IDLE_LOG_RESOURCE_USAGE)
			spm_resource_req_dump();
	}

	return wr;
}



