/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include "mtk_spm_internal.h"
#include <mt-plat/mtk_ccci_common.h> /* exec_ccci_kern_func_by_md_id */
#include <mt-plat/mtk_wd_api.h> /* ap wdt related definitons */

#include <trace/events/mtk_idle_event.h>

#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <mtk_spm_suspend_internal.h>
#include <mtk_spm_resource_req.h>


#include "pwr_ctrl.h"

#include <mtk_idle_module.h>
#include <mtk_idle_module_plat.h>

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

static unsigned int idle_pcm_flags[IDLE_MODEL_NUM] = {
	[IDLE_MODEL_BUS26M] =
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

	[IDLE_MODEL_SYSPLL] =
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

	[IDLE_MODEL_DRAM] =
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
};

static unsigned int idle_pcm_flags1[IDLE_MODEL_NUM] = {
	[IDLE_MODEL_BUS26M] = 0,
	[IDLE_MODEL_SYSPLL] = 0,
	[IDLE_MODEL_DRAM] = 0,
};

#define INVALID_IDLE_TYPE(type) \
	(type != IDLE_MODEL_BUS26M && type != IDLE_MODEL_SYSPLL && \
		type != IDLE_MODEL_DRAM)

struct pwr_ctrl pwrctrl_bus26m;
struct pwr_ctrl pwrctrl_syspll;
struct pwr_ctrl pwrctrl_dram;

static struct pwr_ctrl *get_pwrctrl[IDLE_MODEL_NUM] = {
	[IDLE_MODEL_BUS26M] = &pwrctrl_bus26m,
	[IDLE_MODEL_SYSPLL] = &pwrctrl_syspll,
	[IDLE_MODEL_DRAM] = &pwrctrl_dram,
};

static void mtk_idle_gs_dump(int idle_type)
{
	#if defined(MTK_IDLE_GS_DUMP_READY)
	if (idle_type == IDLE_MODEL_SYSPLL)
		mt_power_gs_dump_dpidle(GS_ALL);
	else if (idle_type == IDLE_MODEL_BUS26M || idle_type == IDLE_MODEL_DRAM)
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
	case IDLE_MODEL_BUS26M:
		trace_sodi3_rcuidle(cpu, enter);
		break;
	case IDLE_MODEL_SYSPLL:
		trace_dpidle_rcuidle(cpu, enter);
		break;
	case IDLE_MODEL_DRAM:
		trace_sodi_rcuidle(cpu, enter);
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
	unsigned int cpuidle_mode[IDLE_MODEL_NUM] = {
		[IDLE_MODEL_BUS26M] = MTK_IDLEBUS26M_MODE,
		[IDLE_MODEL_SYSPLL] = MTK_IDLESYSPLL_MODE,
		[IDLE_MODEL_DRAM] = MTK_IDLEDRAM_MODE,
	};
	unsigned int enter_flag[IDLE_MODEL_NUM] = {
		[IDLE_MODEL_BUS26M] = SPM_ARGS_IDLE_BUS26M,
		[IDLE_MODEL_SYSPLL] = SPM_ARGS_IDLE_SYSPLL,
		[IDLE_MODEL_DRAM] = SPM_ARGS_IDLE_DRAM,
	};
	unsigned int leave_flag[IDLE_MODEL_NUM] = {
		[IDLE_MODEL_BUS26M] = SPM_ARGS_IDLE_BUS26M_FINISH,
		[IDLE_MODEL_SYSPLL] = SPM_ARGS_IDLE_SYSPLL_FINISH,
		[IDLE_MODEL_DRAM] = SPM_ARGS_IDLE_DRAM_FINISH,
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
		printk_deferred("[name:spm&]mtk_enter_idle_state(%d) ret %d\n",
			cpuidle_mode[idle_type], spm_dormant_sta);

	return spm_dormant_sta;
}


/********************************************************************
 * dp/so3/so setup/cleanup pcm
 *******************************************************************/


static void spm_idle_pcm_setup_before_wfi(
	int idle_type, struct pwr_ctrl *pwrctrl, unsigned int op_cond)
{
	unsigned int resource_usage = 0;

	resource_usage = spm_get_resource_usage();

	if (INVALID_IDLE_TYPE(idle_type))
		return;

	mt_secure_call(MTK_SIP_KERNEL_SPM_LP_ARGS,
		(SPM_LP_SMC_MAGIC | idle_type),
		pwrctrl->pcm_flags, pwrctrl->pcm_flags1, resource_usage);

	if (idle_type == IDLE_MODEL_BUS26M)
		mt_secure_call(MTK_SIP_KERNEL_SPM_PWR_CTRL_ARGS
			, SPM_PWR_CTRL_SODI3, PW_WDT_DISABLE, 1, 0);

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

static unsigned long flags;

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

static unsigned int mtk_idle_output_log(
	int idle_type, const struct wake_status *wakesta,
	unsigned int op_cond, unsigned int idle_flag);

typedef unsigned int (*mtk_idle_log_t) (
	int idle_type, const struct wake_status *wakesta,
	unsigned int op_cond, unsigned int idle_flag);

static mtk_idle_log_t mtk_idle_log[IDLE_MODEL_NUM] = {
	[IDLE_MODEL_BUS26M] = mtk_idle_output_log,
	[IDLE_MODEL_SYSPLL] = mtk_idle_output_log,
	[IDLE_MODEL_DRAM] = mtk_idle_output_log,
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

static unsigned int mtk_idle_output_log(
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
		printk_deferred("[name:spm&]Power/swap op_cond = 0x%x\n"
			, op_cond);
		wr = __spm_output_wake_reason(
			wakesta, false, mtk_idle_name(idle_type));
		if (idle_flag & MTK_IDLE_LOG_RESOURCE_USAGE)
			spm_resource_req_dump();
	}

	return wr;
}

