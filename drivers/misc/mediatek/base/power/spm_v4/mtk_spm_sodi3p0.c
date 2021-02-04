/*
 * Copyright (C) 2016 MediaTek Inc.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <mt-plat/mtk_secure_api.h>

#include <mtk_gpt.h>
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
#include <mach/wd_api.h>
#endif

#include <mt-plat/mtk_boot.h>
#if defined(CONFIG_MTK_SYS_CIRQ)
#include <mt-plat/mtk_cirq.h>
#endif
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif

#if defined(CONFIG_MACH_MT6739)
#include <mtk_pmic_api_buck.h>
#include <mt-plat/mtk_rtc.h>
#endif

#include <mt-plat/mtk_io.h>

#include <mtk_spm_sodi3.h>
#include <mtk_spm_resource_req.h>
#include <mtk_spm_resource_req_internal.h>
#include <mtk_spm_pmic_wrap.h>

#include <mtk_power_gs_api.h>

#include <trace/events/mtk_idle_event.h>

#include <mtk_idle_internal.h>
#include <mtk_idle_profile.h>

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_define.h>
#include <sspm_timesync.h>
#endif

/**************************************
 * only for internal debug
 **************************************/
#define PCM_SEC_TO_TICK(sec)	(sec * 32768)
#define SPM_PCMWDT_EN		(0)

#define LOG_BUF_SIZE					(256)
#define SODI3_LOGOUT_TIMEOUT_CRITERIA	(20)
#define SODI3_LOGOUT_INTERVAL_CRITERIA	(5000U) /* unit:ms */

static struct pwr_ctrl sodi3_ctrl;

struct spm_lp_scen __spm_sodi3 = {
	.pwrctrl = &sodi3_ctrl,
};

static bool gSpm_sodi3_en = true;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static void spm_sodi3_notify_sspm_before_wfi(
		struct pwr_ctrl *pwrctrl, u32 operation_cond)
{
	int ret;
	struct spm_data spm_d;
	unsigned int spm_opt = 0;

	memset(&spm_d, 0, sizeof(struct spm_data));

#ifdef SSPM_TIMESYNC_SUPPORT
	sspm_timesync_ts_get(&spm_d.u.suspend.sys_timestamp_h,
			&spm_d.u.suspend.sys_timestamp_l);
	sspm_timesync_clk_get(&spm_d.u.suspend.sys_src_clk_h,
			&spm_d.u.suspend.sys_src_clk_l);
#endif

	spm_opt |= spm_for_gps_flag ?  SPM_OPT_GPS_STAT     : 0;
	spm_opt |= (operation_cond & DEEPIDLE_OPT_VCORE_LP_MODE) ?
		SPM_OPT_VCORE_LP_MODE : 0;
	spm_opt |= (operation_cond & DEEPIDLE_OPT_XO_UFS_ON_OFF) ?
		SPM_OPT_XO_UFS_OFF : 0;

	spm_d.u.suspend.spm_opt = spm_opt;

	ret = spm_to_sspm_command_async(SPM_ENTER_SODI3, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);
}

static void spm_sodi3_notify_sspm_before_wfi_async_wait(void)
{
	int ret = 0;

	ret = spm_to_sspm_command_async_wait(SPM_ENTER_SODI3);
	if (ret < 0)
		spm_crit2("SPM_ENTER_SODI3 async wait: ret %d", ret);
}

static void spm_sodi3_notify_sspm_after_wfi(u32 operation_cond)
{
	int ret;
	struct spm_data spm_d;
	unsigned int spm_opt = 0;

	memset(&spm_d, 0, sizeof(struct spm_data));

#ifdef SSPM_TIMESYNC_SUPPORT
	sspm_timesync_ts_get(&spm_d.u.suspend.sys_timestamp_h,
			&spm_d.u.suspend.sys_timestamp_l);
	sspm_timesync_clk_get(&spm_d.u.suspend.sys_src_clk_h,
			&spm_d.u.suspend.sys_src_clk_l);
#endif

	spm_opt |= (operation_cond & DEEPIDLE_OPT_XO_UFS_ON_OFF) ?
		SPM_OPT_XO_UFS_OFF : 0;

	spm_d.u.suspend.spm_opt = spm_opt;

	ret = spm_to_sspm_command_async(SPM_LEAVE_SODI3, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);
}

static void spm_sodi3_notify_sspm_after_wfi_async_wait(void)
{
	int ret = 0;

	ret = spm_to_sspm_command_async_wait(SPM_LEAVE_SODI3);
	if (ret < 0)
		spm_crit2("SPM_LEAVE_SODI3 async wait: ret %d", ret);
}
#else /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
static void spm_sodi3_notify_sspm_before_wfi(
		struct pwr_ctrl *pwrctrl, u32 operation_cond)
{
#if defined(CONFIG_MACH_MT6739)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	wk_auxadc_bgd_ctrl(0);
	rtc_clock_enable(0);
#endif
#endif
}

static void spm_sodi3_notify_sspm_before_wfi_async_wait(void)
{
}

static void spm_sodi3_notify_sspm_after_wfi(u32 operation_cond)
{
#if defined(CONFIG_MACH_MT6739)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	rtc_clock_enable(1);
	wk_auxadc_bgd_ctrl(1);
#endif
#endif
}

static void spm_sodi3_notify_sspm_after_wfi_async_wait(void)
{
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

static void spm_sodi3_pcm_setup_after_wfi(
		struct pwr_ctrl *pwrctrl, u32 operation_cond)
{
	spm_sodi3_post_process();
}


static void spm_sodi3_setup_wdt(struct pwr_ctrl *pwrctrl, void **api)
{
#if SPM_PCMWDT_EN && defined(CONFIG_MTK_WATCHDOG) \
	&& defined(CONFIG_MTK_WD_KICKER)
	struct wd_api *wd_api = NULL;

	if (!get_wd_api(&wd_api)) {
		wd_api->wd_spmwdt_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);
		wd_api->wd_suspend_notify();
		pwrctrl->wdt_disable = 0;
	} else {
		spm_crit2("FAILED TO GET WD API\n");
		api = NULL;
		pwrctrl->wdt_disable = 1;
	}
	*api = wd_api;
#else
	pwrctrl->wdt_disable = 1;
#endif
}

static void spm_sodi3_resume_wdt(struct pwr_ctrl *pwrctrl, void *api)
{
#if SPM_PCMWDT_EN && defined(CONFIG_MTK_WATCHDOG) \
	&& defined(CONFIG_MTK_WD_KICKER)
	struct wd_api *wd_api = (struct wd_api *)api;

	if (!pwrctrl->wdt_disable && wd_api != NULL) {
		wd_api->wd_resume_notify();
		wd_api->wd_spmwdt_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);
	} else {
		spm_crit2("pwrctrl->wdt_disable %d\n", pwrctrl->wdt_disable);
	}
#endif
}

static void spm_sodi3_atf_time_sync(void)
{
	/* Get local_clock and sync to ATF */
	u64 time_to_sync = local_clock();

#ifdef CONFIG_ARM64
	mt_secure_call(MTK_SIP_KERNEL_TIME_SYNC, time_to_sync, 0, 0, 0);
#else
	mt_secure_call(MTK_SIP_KERNEL_TIME_SYNC,
			(u32)time_to_sync, (u32)(time_to_sync >> 32), 0, 0);
#endif
}

unsigned int spm_go_to_sodi3(u32 spm_flags, u32 spm_data, u32 sodi3_flags)
{
	void *api = NULL;
	struct wake_status wakesta;
	unsigned long flags;
#if defined(CONFIG_MTK_GIC_V3_EXT)
	struct mtk_irq_mask mask;
#endif
	unsigned int wr = WR_NONE;
	struct pcm_desc *pcmdesc = NULL;
	struct pwr_ctrl *pwrctrl = __spm_sodi3.pwrctrl;
	u32 cpu = smp_processor_id();
	u32 spm_flags1 = spm_data;
	unsigned int operation_cond = 0;

	spm_sodi3_footprint(SPM_SODI3_ENTER);

	profile_so3_start(PIDX_PRE_HANDLER);
	operation_cond |= soidle_pre_handler();
	profile_so3_end(PIDX_PRE_HANDLER);

#ifdef SUPPORT_SW_SET_SPM_MEMEPLL_MODE
	if (spm_get_sodi_mempll() == 1)
		spm_flags |= SPM_FLAG_SODI_CG_MODE; /* CG mode */
	else
		spm_flags &= ~SPM_FLAG_SODI_CG_MODE; /* PDN mode */
#endif

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	__sync_big_buck_ctrl_pcm_flag(&spm_flags1);
	__sync_vcore_ctrl_pcm_flag(operation_cond, &spm_flags1);
	set_pwrctrl_pcm_flags1(pwrctrl, spm_flags1);

	pwrctrl->timer_val = PCM_SEC_TO_TICK(2);

	spm_sodi3_setup_wdt(pwrctrl, &api);
	soidle3_before_wfi(cpu);

	spin_lock_irqsave(&__spm_lock, flags);

	spm_sodi3_footprint(SPM_SODI3_ENTER_SSPM_ASYNC_IPI_BEFORE_WFI);

	profile_so3_start(PIDX_SSPM_BEFORE_WFI);
	spm_sodi3_notify_sspm_before_wfi(pwrctrl, operation_cond);
	profile_so3_end(PIDX_SSPM_BEFORE_WFI);

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep_ex(SPM_IRQ0_ID);
	unmask_edge_trig_irqs_for_cirq();
#endif

	profile_so3_start(PIDX_PRE_IRQ_PROCESS);
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_clone_gic();
	mt_cirq_enable();
#endif
	profile_so3_end(PIDX_PRE_IRQ_PROCESS);

	spm_sodi3_footprint(SPM_SODI3_ENTER_SPM_FLOW);

	profile_so3_start(PIDX_PCM_SETUP_BEFORE_WFI);
	spm_sodi3_pcm_setup_before_wfi(cpu, pcmdesc, pwrctrl, operation_cond);
	profile_so3_end(PIDX_PCM_SETUP_BEFORE_WFI);

	profile_so3_start(PIDX_SSPM_BEFORE_WFI_ASYNC_WAIT);
	spm_sodi3_notify_sspm_before_wfi_async_wait();
	profile_so3_end(PIDX_SSPM_BEFORE_WFI_ASYNC_WAIT);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	spm_sodi3_footprint(SPM_SODI3_ENTER_UART_SLEEP);

	if (!(sodi3_flags & SODI_FLAG_DUMP_LP_GS)) {
#if defined(CONFIG_MACH_MT6771)
		if (mtk8250_request_to_sleep()) {
#else
		if (request_uart_to_sleep()) {
#endif
			wr = WR_UART_BUSY;
			goto RESTORE_IRQ;
		}
	}

	if (sodi3_flags & SODI_FLAG_DUMP_LP_GS)
		mt_power_gs_dump_sodi3(GS_ALL);
#endif

	spm_sodi3_footprint_val((1 << SPM_SODI3_ENTER_WFI) |
		(1 << SPM_SODI3_B4) |
		(1 << SPM_SODI3_B5) | (1 << SPM_SODI3_B6));

	trace_sodi3_rcuidle(cpu, 1);

	profile_so3_end(PIDX_ENTER_TOTAL);

	spm_trigger_wfi_for_sodi(pwrctrl->pcm_flags);

	profile_so3_start(PIDX_LEAVE_TOTAL);

	trace_sodi3_rcuidle(cpu, 0);

	spm_sodi3_footprint(SPM_SODI3_LEAVE_WFI);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (!(sodi3_flags & SODI_FLAG_DUMP_LP_GS))
#if defined(CONFIG_MACH_MT6771)
		mtk8250_request_to_wakeup();
#else
		request_uart_to_wakeup();
#endif
RESTORE_IRQ:

	spm_sodi3_footprint(SPM_SODI3_ENTER_UART_AWAKE);
#endif

	profile_so3_start(PIDX_SSPM_AFTER_WFI);
	spm_sodi3_notify_sspm_after_wfi(operation_cond);
	profile_so3_end(PIDX_SSPM_AFTER_WFI);

	spm_sodi3_footprint(SPM_SODI3_LEAVE_SSPM_ASYNC_IPI_AFTER_WFI);

	__spm_get_wakeup_status(&wakesta);

	profile_so3_start(PIDX_PCM_SETUP_AFTER_WFI);
	spm_sodi3_pcm_setup_after_wfi(pwrctrl, operation_cond);
	profile_so3_end(PIDX_PCM_SETUP_AFTER_WFI);

	wr = spm_sodi_output_log(&wakesta,
			pcmdesc, sodi3_flags | SODI_FLAG_3P0, operation_cond);

	spm_sodi3_footprint(SPM_SODI3_LEAVE_SPM_FLOW);

	profile_so3_start(PIDX_POST_IRQ_PROCESS);
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_flush();
	mt_cirq_disable();
#endif
	profile_so3_end(PIDX_POST_IRQ_PROCESS);

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_restore(&mask);
#endif

	spin_unlock_irqrestore(&__spm_lock, flags);

	soidle3_after_wfi(cpu);

	spm_sodi3_footprint(SPM_SODI3_LEAVE);

	profile_so3_start(PIDX_POST_HANDLER);
	soidle_post_handler();
	profile_so3_end(PIDX_POST_HANDLER);

	spm_sodi3_resume_wdt(pwrctrl, api);

	spm_sodi3_atf_time_sync();

	profile_so3_start(PIDX_SSPM_AFTER_WFI_ASYNC_WAIT);
	spm_sodi3_notify_sspm_after_wfi_async_wait();
	profile_so3_end(PIDX_SSPM_AFTER_WFI_ASYNC_WAIT);

	spm_sodi3_reset_footprint();

#if 1
	if (wr == WR_PCM_ASSERT)
		rekick_vcorefs_scenario();
#endif

	return wr;
}

void spm_enable_sodi3(bool en)
{
	gSpm_sodi3_en = en;
}

bool spm_get_sodi3_en(void)
{
	return gSpm_sodi3_en;
}

void spm_sodi3_init(void)
{
	sodi3_debug("spm sodi3 init\n");
	spm_sodi3_aee_init();
}

MODULE_DESCRIPTION("SPM-SODI3 Driver v0.1");
