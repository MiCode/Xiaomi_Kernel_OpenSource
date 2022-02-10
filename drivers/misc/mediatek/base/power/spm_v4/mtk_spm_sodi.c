/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <mt-plat/mtk_secure_api.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mtk_gpt.h>
#include <mt-plat/mtk_boot.h>
#if defined(CONFIG_MTK_SYS_CIRQ)
#include <mt-plat/mtk_cirq.h>
#endif
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif

#if defined(CONFIG_MACH_MT6739)
#include <mtk_clkbuf_ctl.h>
#include "pmic_api_buck.h"
#include <mt-plat/mtk_rtc.h>
#endif

#include <mt-plat/mtk_io.h>

#include <mtk_spm_sodi.h>
#include <mtk_spm_resource_req.h>
#include <mtk_spm_resource_req_internal.h>
#include <mtk_spm_pmic_wrap.h>

#if !defined(SPM_K414_EARLY_PORTING)
#include <mtk_power_gs_api.h>
#endif

#include <mtk_idle_internal.h>
#include <mtk_idle_profile.h>

void __attribute__ ((weak)) mtk8250_backup_dev(void)
{
	//pr_debug("NO %s !!!\n", __func__);
}

void __attribute__ ((weak)) mtk8250_restore_dev(void)
{
	//pr_debug("NO %s !!!\n", __func__);
}

int __attribute__ ((weak)) mtk8250_request_to_wakeup(void)
{
	//pr_debug("NO %s !!!\n", __func__);
	return 0;
}

int __attribute__ ((weak)) mtk8250_request_to_sleep(void)
{
	//pr_debug("NO %s !!!\n", __func__);
	return 0;
}

/**************************************
 * only for internal debug
 **************************************/

enum spm_sodi_logout_reason {
	SODI_LOGOUT_NONE = 0,
	SODI_LOGOUT_ASSERT = 1,
	SODI_LOGOUT_NOT_GPT_EVENT = 2,
	SODI_LOGOUT_RESIDENCY_ABNORMAL = 3,
	SODI_LOGOUT_EMI_STATE_CHANGE = 4,
	SODI_LOGOUT_LONG_INTERVAL = 5,
	SODI_LOGOUT_CG_PD_STATE_CHANGE = 6,
	SODI_LOGOUT_UNKNOWN = -1,
};

#define LOG_BUF_SIZE					(256)
#define SODI_LOGOUT_TIMEOUT_CRITERIA	(20)
#define SODI_LOGOUT_INTERVAL_CRITERIA	(5000U)	/* unit:ms */

static struct pwr_ctrl sodi_ctrl;

struct spm_lp_scen __spm_sodi = {
	.pwrctrl = &sodi_ctrl,
};

/* 0:power-down mode, 1:CG mode */
static bool gSpm_SODI_mempll_pwr_mode;
static bool gSpm_sodi_en;
static bool gSpm_lcm_vdo_mode;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static void spm_sodi_notify_sspm_before_wfi(struct pwr_ctrl *pwrctrl,
					    u32 operation_cond)
{
	int ret;
	struct spm_data spm_d;
	unsigned int spm_opt = 0;

	memset(&spm_d, 0, sizeof(struct spm_data));

	spm_opt |= spm_for_gps_flag ?  SPM_OPT_GPS_STAT     : 0;
	spm_opt |= (operation_cond & DEEPIDLE_OPT_XO_UFS_ON_OFF) ?
		SPM_OPT_XO_UFS_OFF : 0;
	spm_opt |= (operation_cond & DEEPIDLE_OPT_CLKBUF_BBLPM) ?
		SPM_OPT_CLKBUF_ENTER_BBLPM : 0;

	spm_d.u.suspend.spm_opt = spm_opt;

	ret = spm_to_sspm_command_async(SPM_ENTER_SODI, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);
}

static void spm_sodi_notify_sspm_before_wfi_async_wait(void)
{
	int ret = 0;

	ret = spm_to_sspm_command_async_wait(SPM_ENTER_SODI);
	if (ret < 0)
		spm_crit2("SPM_ENTER_SODI async wait: ret %d", ret);
}

static void spm_sodi_notify_sspm_after_wfi(u32 operation_cond)
{
	int ret;
	struct spm_data spm_d;
	unsigned int spm_opt = 0;

	memset(&spm_d, 0, sizeof(struct spm_data));

	spm_opt |= (operation_cond & DEEPIDLE_OPT_XO_UFS_ON_OFF) ?
		SPM_OPT_XO_UFS_OFF : 0;
	spm_opt |= (operation_cond & DEEPIDLE_OPT_CLKBUF_BBLPM) ?
		SPM_OPT_CLKBUF_ENTER_BBLPM : 0;

	spm_d.u.suspend.spm_opt = spm_opt;

	ret = spm_to_sspm_command_async(SPM_LEAVE_SODI, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);
}

static void spm_sodi_notify_sspm_after_wfi_async_wait(void)
{
	int ret = 0;

	ret = spm_to_sspm_command_async_wait(SPM_LEAVE_SODI);
	if (ret < 0)
		spm_crit2("SPM_LEAVE_SODI async wait: ret %d", ret);
}
#else /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
static void spm_sodi_notify_sspm_before_wfi(struct pwr_ctrl *pwrctrl,
					    u32 operation_cond)
{
#if defined(CONFIG_MACH_MT6739)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	wk_auxadc_bgd_ctrl(0);
	rtc_clock_enable(0);

/* 	if (operation_cond & DEEPIDLE_OPT_CLKBUF_BBLPM)
		clk_buf_control_bblpm_temp(1); */
#endif
#endif
}

static void spm_sodi_notify_sspm_before_wfi_async_wait(void)
{
}

static void spm_sodi_notify_sspm_after_wfi(u32 operation_cond)
{
#if defined(CONFIG_MACH_MT6739)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
/* 	if (operation_cond & DEEPIDLE_OPT_CLKBUF_BBLPM)
		clk_buf_control_bblpm_temp(0); */

	rtc_clock_enable(1);
	wk_auxadc_bgd_ctrl(1);
#endif
#endif
}

static void spm_sodi_notify_sspm_after_wfi_async_wait(void)
{
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

void spm_trigger_wfi_for_sodi(u32 pcm_flags)
{
	int spm_dormant_sta = 0;

	if (is_cpu_pdn(pcm_flags))
		spm_dormant_sta = mtk_enter_idle_state(MTK_SODI_MODE);
	else {
		SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS,
			       SPM_ARGS_SODI, 0, 0);
		SMC_CALL(MTK_SIP_KERNEL_SPM_LEGACY_SLEEP,
			       0, 0, 0);
		SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS,
			       SPM_ARGS_SODI_FINISH, 0, 0);
	}

	if (spm_dormant_sta < 0)
		sodi_err("spm_dormant_sta(%d) < 0\n", spm_dormant_sta);
}

static void spm_sodi_pcm_setup_after_wfi(u32 operation_cond)
{
	spm_sodi_post_process();
}

static inline bool spm_sodi_assert(struct wake_status *wakesta)
{
	return (wakesta->assert_pc != 0) || (wakesta->r12 == 0);
}

static bool spm_sodi_is_not_gpt_event(
	struct wake_status *wakesta, long int curr_time, long int prev_time)
{
	static int by_md2ap_count;
	bool logout = false;

	if ((wakesta->r12 & R12_SYS_TIMER_EVENT_B) == 0) {
		if (wakesta->r12 & R12_MD2AP_PEER_EVENT_B) {
			/* wake up by R12_MD2AP_PEER_EVENT_B */
			if ((by_md2ap_count >= 5) ||
			    ((curr_time - prev_time) > 20U)) {
				logout = true;
				by_md2ap_count = 0;
			} else if (by_md2ap_count == 0) {
				logout = true;
			}
			by_md2ap_count++;
		} else {
			logout = true;
		}
	}
	return logout;
}

static inline bool spm_sodi_abnormal_residency(struct wake_status *wakesta)
{
	return (wakesta->timer_out <= SODI_LOGOUT_TIMEOUT_CRITERIA);
}

static inline bool spm_sodi_change_emi_state(struct wake_status *wakesta,
					     int pre_emi_cnt)
{
	return (spm_read(SPM_PASR_DPD_0) == 0 && pre_emi_cnt > 0) ||
				(spm_read(SPM_PASR_DPD_0) > 0 &&
				 pre_emi_cnt == 0);
}

static inline bool spm_sodi_last_logout(long int curr_time,
					long int prev_time)
{
	return (curr_time - prev_time) > SODI_LOGOUT_INTERVAL_CRITERIA;
}

static inline bool spm_sodi_memPllCG(void)
{
	return ((spm_read(SPM_SW_FLAG) & SPM_FLAG_SODI_CG_MODE) != 0) ||
			((spm_read(DUMMY1_PWR_CON) &
			  DUMMY1_PWR_ISO_LSB) != 0);
}

static bool spm_sodi_mem_mode_change(void)
{
	static int memPllCG_prev_status = 1;	/* 1:CG, 0:pwrdn */
	bool logout = false;
	int mem_status = 0;

	if (((spm_read(SPM_SW_FLAG) & SPM_FLAG_SODI_CG_MODE) != 0) ||
		((spm_read(DUMMY1_PWR_CON) & DUMMY1_PWR_ISO_LSB) != 0))
		mem_status = 1;

	if (memPllCG_prev_status != mem_status) {
		memPllCG_prev_status = mem_status;
		logout = true;
	}
	return logout;
}

static void spm_sodi_append_log(char *buf, const char *local_ptr)
{
	if ((strlen(buf) + strlen(local_ptr)) < LOG_BUF_SIZE)
		strncat(buf, local_ptr, strlen(local_ptr));
}

unsigned int spm_sodi_output_log(struct wake_status *wakesta,
	struct pcm_desc *pcmdesc, u32 flags, u32 operation_cond)
{
	static unsigned long int sodi_logout_prev_time;
	static int pre_emi_refresh_cnt;
	static unsigned int logout_sodi_cnt;
	static unsigned int logout_selfrefresh_cnt;

	unsigned int wr = WR_NONE;
	unsigned long int sodi_logout_curr_time = 0;
	int need_log_out = 0;

	if (mtk_idle_latency_profile_is_on())
		return wr;

	if (!(flags & SODI_FLAG_REDUCE_LOG) || (flags & SODI_FLAG_RESIDENCY)) {

		so_warn(flags,
			"self_refresh = 0x%x, sw_flag = 0x%x, 0x%x, oper_cond = 0x%x\n",
			spm_read(SPM_PASR_DPD_0), spm_read(SPM_SW_FLAG),
			spm_read(DUMMY1_PWR_CON), operation_cond);
		wr = __spm_output_wake_reason(wakesta, pcmdesc, false, "sodi");

		if (flags & SODI_FLAG_RESOURCE_USAGE)
			spm_resource_req_dump();
	} else {

		sodi_logout_curr_time = spm_get_current_time_ms();

		if (spm_sodi_assert(wakesta))
			need_log_out = SODI_LOGOUT_ASSERT;
		else if (spm_sodi_is_not_gpt_event(wakesta,
						   sodi_logout_curr_time,
						   sodi_logout_prev_time))
			need_log_out = SODI_LOGOUT_NOT_GPT_EVENT;
		else if (spm_sodi_abnormal_residency(wakesta))
			need_log_out = SODI_LOGOUT_RESIDENCY_ABNORMAL;
		else if (spm_sodi_change_emi_state(wakesta,
						   pre_emi_refresh_cnt))
			need_log_out = SODI_LOGOUT_EMI_STATE_CHANGE;
		else if (spm_sodi_last_logout(sodi_logout_curr_time,
					      sodi_logout_prev_time))
			need_log_out = SODI_LOGOUT_LONG_INTERVAL;
		else if (spm_sodi_mem_mode_change())
			need_log_out = SODI_LOGOUT_CG_PD_STATE_CHANGE;

		logout_sodi_cnt++;
		logout_selfrefresh_cnt += spm_read(SPM_PASR_DPD_0);
		pre_emi_refresh_cnt = spm_read(SPM_PASR_DPD_0);

		if (need_log_out != SODI_LOGOUT_NONE) {
			sodi_logout_prev_time = sodi_logout_curr_time;

			if (need_log_out == SODI_LOGOUT_ASSERT) {

				if (wakesta->assert_pc != 0) {
					so_err(flags,
					       "Warning: wakeup reason is WR_PCM_ASSERT!\n");
					wr = WR_PCM_ASSERT;
				} else if (wakesta->r12 == 0) {
					so_err(flags,
					       "Warning: wakeup reason is WR_UNKNOWN!\n");
					wr = WR_UNKNOWN;
				}
				so_err(flags, "SELF_REFRESH = 0x%x, SW_FLAG = 0x%x, 0x%x, SODI_CNT = %d, SELF_REFRESH_CNT = 0x%x, ASSERT_PC = 0x%0x, R13 = 0x%x, DEBUG_FLAG = 0x%x, R12 = 0x%x, R12_E = 0x%x, RAW_STA = 0x%x, IDLE_STA = 0x%x, EVENT_REG = 0x%x, ISR = 0x%x\n",
						spm_read(SPM_PASR_DPD_0),
						spm_read(SPM_SW_FLAG),
						spm_read(DUMMY1_PWR_CON),
						logout_sodi_cnt,
						logout_selfrefresh_cnt,
						wakesta->assert_pc,
						wakesta->r13,
						wakesta->debug_flag,
						wakesta->r12,
						wakesta->r12_ext,
						wakesta->raw_sta,
						wakesta->idle_sta,
						wakesta->event_reg,
						wakesta->isr);
				wr = WR_PCM_ASSERT;

			} else {
				char buf[LOG_BUF_SIZE] = { 0 };
				int i;

				if (wakesta->r12 & WAKE_SRC_R12_PCM_TIMER) {
					if (wakesta->wake_misc &
					    WAKE_MISC_PCM_TIMER)
						spm_sodi_append_log(buf,
							" PCM_TIMER");

					if (wakesta->wake_misc & WAKE_MISC_TWAM)
						spm_sodi_append_log(buf,
							" TWAM");

					if (wakesta->wake_misc &
					    WAKE_MISC_CPU_WAKE)
						spm_sodi_append_log(buf,
							" CPU");
				}

				for (i = 1; i < 32; i++) {
					if (wakesta->r12 & (1U << i)) {
						spm_sodi_append_log(buf,
							wakesrc_str[i]);
						wr = WR_WAKE_SRC;
					}
				}
				WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

				so_warn(flags, "wake up by %s, self_refresh = 0x%x, sw_flag = 0x%x, 0x%x, %d, 0x%x, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, %d, %08x\n",
						buf, spm_read(SPM_PASR_DPD_0),
						spm_read(SPM_SW_FLAG),
						spm_read(DUMMY1_PWR_CON),
						logout_sodi_cnt,
						logout_selfrefresh_cnt,
						wakesta->timer_out,
						wakesta->r13,
						wakesta->debug_flag,
						wakesta->r12,
						wakesta->r12_ext,
						wakesta->raw_sta,
						wakesta->idle_sta,
						wakesta->event_reg,
						wakesta->isr,
						spm_get_resource_usage(),
						need_log_out, wakesta->req_sta);
			}
			logout_sodi_cnt = 0;
			logout_selfrefresh_cnt = 0;
		}
	}

	return wr;
}

unsigned int spm_go_to_sodi(u32 spm_flags, u32 spm_data, u32 sodi_flags)
{
	struct wake_status wakesta;
	unsigned long flags;
#if defined(CONFIG_MTK_GIC_V3_EXT)
	struct mtk_irq_mask mask;
#endif
	unsigned int wr = WR_NONE;
	struct pcm_desc *pcmdesc = NULL;
	struct pwr_ctrl *pwrctrl = __spm_sodi.pwrctrl;
	u32 cpu = smp_processor_id();
	u32 spm_flags1 = spm_data;
	unsigned int operation_cond = 0;

	spm_sodi_footprint(SPM_SODI_ENTER);

	profile_so_start(PIDX_PRE_HANDLER);
	operation_cond |= soidle_pre_handler();
	profile_so_end(PIDX_PRE_HANDLER);

#ifdef SUPPORT_SW_SET_SPM_MEMEPLL_MODE
	if (spm_get_sodi_mempll() == 1)
		spm_flags |= SPM_FLAG_SODI_CG_MODE; /* CG mode */
	else
		spm_flags &= ~SPM_FLAG_SODI_CG_MODE; /* PDN mode */
#endif

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	__sync_big_buck_ctrl_pcm_flag(&spm_flags1);
	set_pwrctrl_pcm_flags1(pwrctrl, spm_flags1);

	soidle_before_wfi(cpu);

	spin_lock_irqsave(&__spm_lock, flags);

	spm_sodi_footprint(SPM_SODI_ENTER_SSPM_ASYNC_IPI_BEFORE_WFI);

	profile_so_start(PIDX_SSPM_BEFORE_WFI);
	spm_sodi_notify_sspm_before_wfi(pwrctrl, operation_cond);
	profile_so_end(PIDX_SSPM_BEFORE_WFI);

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep_ex(SPM_IRQ0_ID);
	unmask_edge_trig_irqs_for_cirq();
#endif

	profile_so_start(PIDX_PRE_IRQ_PROCESS);
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_clone_gic();
	mt_cirq_enable();
#endif
	profile_so_end(PIDX_PRE_IRQ_PROCESS);

	spm_sodi_footprint(SPM_SODI_ENTER_SPM_FLOW);

	profile_so_start(PIDX_PCM_SETUP_BEFORE_WFI);
	spm_sodi_pcm_setup_before_wfi(cpu, pcmdesc, pwrctrl, operation_cond);
	profile_so_end(PIDX_PCM_SETUP_BEFORE_WFI);

	profile_so_start(PIDX_SSPM_BEFORE_WFI_ASYNC_WAIT);
	spm_sodi_notify_sspm_before_wfi_async_wait();
	profile_so_end(PIDX_SSPM_BEFORE_WFI_ASYNC_WAIT);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	spm_sodi_footprint(SPM_SODI_ENTER_UART_SLEEP);

	if (!(sodi_flags & SODI_FLAG_DUMP_LP_GS)) {
#if defined(CONFIG_MACH_MT6771)
		if (mtk8250_request_to_sleep()) {
#else
		if (request_uart_to_sleep()) {
#endif
			wr = WR_UART_BUSY;
			goto RESTORE_IRQ;
		}
	}
#if !defined(SPM_K414_EARLY_PORTING)
	if (sodi_flags & SODI_FLAG_DUMP_LP_GS)
		mt_power_gs_dump_sodi3(GS_ALL);
#endif
#endif

	spm_sodi_footprint_val((1 << SPM_SODI_ENTER_WFI) |
		(1 << SPM_SODI_B4) | (1 << SPM_SODI_B5) | (1 << SPM_SODI_B6));

	profile_so_end(PIDX_ENTER_TOTAL);

	spm_trigger_wfi_for_sodi(pwrctrl->pcm_flags);

	profile_so_start(PIDX_LEAVE_TOTAL);

	spm_sodi_footprint(SPM_SODI_LEAVE_WFI);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (!(sodi_flags & SODI_FLAG_DUMP_LP_GS))
#if defined(CONFIG_MACH_MT6771)
		mtk8250_request_to_wakeup();
#else
		request_uart_to_wakeup();
#endif
RESTORE_IRQ:

	spm_sodi_footprint(SPM_SODI_ENTER_UART_AWAKE);
#endif

	profile_so_start(PIDX_SSPM_AFTER_WFI);
	spm_sodi_notify_sspm_after_wfi(operation_cond);
	profile_so_end(PIDX_SSPM_AFTER_WFI);

	spm_sodi_footprint(SPM_SODI_LEAVE_SSPM_ASYNC_IPI_AFTER_WFI);

	__spm_get_wakeup_status(&wakesta);

	profile_so_start(PIDX_PCM_SETUP_AFTER_WFI);
	spm_sodi_pcm_setup_after_wfi(operation_cond);
	profile_so_end(PIDX_PCM_SETUP_AFTER_WFI);

	wr = spm_sodi_output_log(&wakesta, pcmdesc, sodi_flags, operation_cond);

	spm_sodi_footprint(SPM_SODI_LEAVE_SPM_FLOW);

	profile_so_start(PIDX_POST_IRQ_PROCESS);
#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_flush();
	mt_cirq_disable();
#endif
	profile_so_end(PIDX_POST_IRQ_PROCESS);

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_restore(&mask);
#endif

	spin_unlock_irqrestore(&__spm_lock, flags);

	soidle_after_wfi(cpu);

	spm_sodi_footprint(SPM_SODI_LEAVE);

	profile_so_start(PIDX_POST_HANDLER);
	soidle_post_handler();
	profile_so_end(PIDX_POST_HANDLER);

	profile_so_start(PIDX_SSPM_AFTER_WFI_ASYNC_WAIT);
	spm_sodi_notify_sspm_after_wfi_async_wait();
	profile_so_end(PIDX_SSPM_AFTER_WFI_ASYNC_WAIT);

	spm_sodi_reset_footprint();

#if 1
	if (wr == WR_PCM_ASSERT)
		rekick_vcorefs_scenario();
#endif

	return wr;
}

void spm_sodi_set_vdo_mode(bool vdo_mode)
{
	gSpm_lcm_vdo_mode = vdo_mode;
}

bool spm_get_cmd_mode(void)
{
	return !gSpm_lcm_vdo_mode;
}

void spm_sodi_mempll_pwr_mode(bool pwr_mode)
{
	gSpm_SODI_mempll_pwr_mode = pwr_mode;
}

bool spm_get_sodi_mempll(void)
{
	return gSpm_SODI_mempll_pwr_mode;
}

void spm_enable_sodi(bool en)
{
	gSpm_sodi_en = en;
}

bool spm_get_sodi_en(void)
{
	return gSpm_sodi_en;
}

void spm_sodi_init(void)
{
	spm_sodi_aee_init();
}

MODULE_DESCRIPTION("SPM-SODI Driver v0.1");
