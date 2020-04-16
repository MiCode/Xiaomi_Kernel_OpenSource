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
#include <linux/of_fdt.h>
#include <mt-plat/mtk_secure_api.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

/* #include <mach/irqs.h> */
#if defined(CONFIG_MTK_SYS_CIRQ)
#include <mt-plat/mtk_cirq.h>
#endif
#include <mtk_spm_idle.h>
#include <mtk_cpuidle.h>
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
#include <mach/wd_api.h>
#endif
#include <mtk_gpt.h>

#ifdef CONFIG_MTK_CCCI_DEVICES
#include <mt-plat/mtk_ccci_common.h>
#endif

#include <mtk_spm_misc.h>
#if defined(CONFIG_MTK_PMIC) || defined(CONFIG_MTK_PMIC_NEW_ARCH)
#include <mt-plat/upmu_common.h>
#endif

#if defined(CONFIG_MACH_MT6739)
#include <mtk_clkbuf_ctl.h>
#include "pmic_api_buck.h"
#include <mt-plat/mtk_rtc.h>
#endif

#include <mtk_idle_internal.h>
#include <mtk_idle_profile.h>
#include <mtk_spm_dpidle.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_pmic_wrap.h>
#include <mtk_spm_resource_req.h>
#include <mtk_spm_resource_req_internal.h>

#if !defined(SPM_K414_EARLY_PORTING)
#include <mtk_power_gs_api.h>
#endif

#include <trace/events/mtk_idle_event.h>

#include <mt-plat/mtk_io.h>

#include <mtk_mcdi_api.h>

/*
 * only for internal debug
 */
#define DPIDLE_TAG     "[name:spm&][DP] "
#define dpidle_dbg(fmt, args...)	printk_deferred(DPIDLE_TAG fmt, ##args)

#define SPM_PWAKE_EN            1
#define SPM_PCMWDT_EN           1

#define I2C_CHANNEL 2

#define spm_is_wakesrc_invalid(wakesrc) \
	(!!((u32)(wakesrc) & 0xc0003803))

/* (CA7MCUCFG_BASE + 0x1C) - 0x1020011c */
#define CA70_BUS_CONFIG          0xF020002C
/* (CA7MCUCFG_BASE + 0x1C) - 0x1020011c */
#define CA71_BUS_CONFIG          0xF020022C

#define SPM_USE_TWAM_DEBUG	0

#define	DPIDLE_LOG_PRINT_TIMEOUT_CRITERIA	20
#define	DPIDLE_LOG_DISCARD_CRITERIA			5000	/* ms */

#define reg_read(addr)         __raw_readl(IOMEM(addr))

enum spm_deepidle_step {
	SPM_DEEPIDLE_ENTER = 0x00000001,
	SPM_DEEPIDLE_ENTER_UART_SLEEP = 0x00000003,
	SPM_DEEPIDLE_ENTER_SSPM_ASYNC_IPI_BEFORE_WFI = 0x00000007,
	SPM_DEEPIDLE_ENTER_WFI = 0x000000ff,
	SPM_DEEPIDLE_LEAVE_WFI = 0x000001ff,
	SPM_DEEPIDLE_LEAVE_SSPM_ASYNC_IPI_AFTER_WFI = 0x000003ff,
	SPM_DEEPIDLE_ENTER_UART_AWAKE = 0x000007ff,
	SPM_DEEPIDLE_LEAVE = 0x00000fff,
	SPM_DEEPIDLE_SLEEP_DPIDLE = 0x80000000
};

#define CPU_FOOTPRINT_SHIFT 24

static int spm_dormant_sta;
static u32 cpu_footprint;

static inline void spm_dpidle_footprint(enum spm_deepidle_step step)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_deepidle_val(aee_rr_curr_deepidle_val() |
				step | cpu_footprint);
#endif
}

static inline void spm_dpidle_reset_footprint(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_deepidle_val(0);
#endif
}

static struct pwr_ctrl dpidle_ctrl;

struct spm_lp_scen __spm_dpidle = {
	.pwrctrl	= &dpidle_ctrl,
};

static unsigned int dpidle_log_discard_cnt;
static unsigned int dpidle_log_print_prev_time;

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static void spm_dpidle_notify_sspm_before_wfi(bool sleep_dpidle,
					      u32 operation_cond,
					      struct pwr_ctrl *pwrctrl)
{
	int ret;
	struct spm_data spm_d;
	unsigned int spm_opt = 0;

	memset(&spm_d, 0, sizeof(struct spm_data));

	spm_opt |= sleep_dpidle ?      SPM_OPT_SLEEP_DPIDLE : 0;
	spm_opt |= spm_for_gps_flag ?  SPM_OPT_GPS_STAT     : 0;
	spm_opt |= (operation_cond & DEEPIDLE_OPT_VCORE_LP_MODE) ?
			SPM_OPT_VCORE_LP_MODE : 0;
	spm_opt |= ((operation_cond & DEEPIDLE_OPT_XO_UFS_ON_OFF) &&
			!sleep_dpidle) ?
			SPM_OPT_XO_UFS_OFF : 0;
	spm_opt |= ((operation_cond & DEEPIDLE_OPT_CLKBUF_BBLPM) &&
			!sleep_dpidle) ?
			SPM_OPT_CLKBUF_ENTER_BBLPM : 0;

	spm_d.u.suspend.spm_opt = spm_opt;

	ret = spm_to_sspm_command_async(SPM_DPIDLE_ENTER, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);
}

static void spm_dpidle_notify_sspm_before_wfi_async_wait(void)
{
	int ret = 0;

	ret = spm_to_sspm_command_async_wait(SPM_DPIDLE_ENTER);
	if (ret < 0)
		spm_crit2("SPM_DPIDLE_ENTER async wait: ret %d", ret);
}

static void spm_dpidle_notify_sspm_after_wfi(bool sleep_dpidle,
					     u32 operation_cond)
{
	int ret;
	struct spm_data spm_d;
	unsigned int spm_opt = 0;

	memset(&spm_d, 0, sizeof(struct spm_data));

	spm_opt |= sleep_dpidle ?      SPM_OPT_SLEEP_DPIDLE : 0;
	spm_opt |= ((operation_cond & DEEPIDLE_OPT_XO_UFS_ON_OFF) &&
			!sleep_dpidle) ?
			SPM_OPT_XO_UFS_OFF : 0;
	spm_opt |= ((operation_cond & DEEPIDLE_OPT_CLKBUF_BBLPM) &&
			!sleep_dpidle) ?
			SPM_OPT_CLKBUF_ENTER_BBLPM : 0;

	spm_d.u.suspend.spm_opt = spm_opt;

	ret = spm_to_sspm_command_async(SPM_DPIDLE_LEAVE, &spm_d);
	if (ret < 0)
		spm_crit2("ret %d", ret);
}

void spm_dpidle_notify_sspm_after_wfi_async_wait(void)
{
	int ret = 0;

	ret = spm_to_sspm_command_async_wait(SPM_DPIDLE_LEAVE);
	if (ret < 0)
		spm_crit2("SPM_DPIDLE_LEAVE async wait: ret %d", ret);
}
#else
static void spm_dpidle_notify_sspm_before_wfi(bool sleep_dpidle,
					      u32 operation_cond,
					      struct pwr_ctrl *pwrctrl)
{
#if defined(CONFIG_MACH_MT6739)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	wk_auxadc_bgd_ctrl(0);
	rtc_clock_enable(0);

	if ((operation_cond & DEEPIDLE_OPT_CLKBUF_BBLPM) && !sleep_dpidle)
		clk_buf_control_bblpm(1);
#endif
#endif
}

static void spm_dpidle_notify_sspm_before_wfi_async_wait(void)
{
}

static void spm_dpidle_notify_sspm_after_wfi(bool sleep_dpidle,
					     u32 operation_cond)
{
#if defined(CONFIG_MACH_MT6739)
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if ((operation_cond & DEEPIDLE_OPT_CLKBUF_BBLPM) && !sleep_dpidle)
		clk_buf_control_bblpm(0);

	rtc_clock_enable(1);
	wk_auxadc_bgd_ctrl(1);
#endif
#endif
}

void spm_dpidle_notify_sspm_after_wfi_async_wait(void)
{
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

static void spm_trigger_wfi_for_dpidle(struct pwr_ctrl *pwrctrl)
{
	if (is_cpu_pdn(pwrctrl->pcm_flags))
		spm_dormant_sta = mtk_enter_idle_state(MTK_DPIDLE_MODE);
	else {
		SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS,
			       SPM_ARGS_DPIDLE, 0, 0);
		SMC_CALL(MTK_SIP_KERNEL_SPM_LEGACY_SLEEP, 0, 0, 0);
		SMC_CALL(MTK_SIP_KERNEL_SPM_ARGS,
			       SPM_ARGS_DPIDLE_FINISH, 0, 0);
	}

	if (spm_dormant_sta < 0)
		printk_deferred("[name:spm&]dpidle spm_dormant_sta(%d) < 0\n",
				spm_dormant_sta);
}

static void spm_dpidle_pcm_setup_after_wfi(bool sleep_dpidle,
					   u32 operation_cond)
{
	spm_dpidle_post_process();
}

/*
 * wakesrc: WAKE_SRC_XXX
 * enable : enable or disable @wakesrc
 * replace: if true, will replace the default setting
 */
int spm_set_dpidle_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	unsigned long flags;

	if (spm_is_wakesrc_invalid(wakesrc))
		return -EINVAL;

	spin_lock_irqsave(&__spm_lock, flags);
	if (enable) {
		if (replace)
			__spm_dpidle.pwrctrl->wake_src = wakesrc;
		else
			__spm_dpidle.pwrctrl->wake_src |= wakesrc;
	} else {
		if (replace)
			__spm_dpidle.pwrctrl->wake_src = 0;
		else
			__spm_dpidle.pwrctrl->wake_src &= ~wakesrc;
	}
	spin_unlock_irqrestore(&__spm_lock, flags);

	return 0;
}

static unsigned int spm_output_wake_reason(struct wake_status *wakesta,
					   struct pcm_desc *pcmdesc,
					   u32 log_cond,
					   u32 operation_cond)
{
	unsigned int wr = WR_NONE;
	unsigned long int dpidle_log_print_curr_time = 0;
	bool log_print = false;
	static bool timer_out_too_short;

	if (log_cond & DEEPIDLE_LOG_FULL) {
		wr = __spm_output_wake_reason(wakesta, pcmdesc,
					      false, "dpidle");
		printk_deferred("[name:spm&]oper_cond = %x\n", operation_cond);

		if (log_cond & DEEPIDLE_LOG_RESOURCE_USAGE)
			spm_resource_req_dump();
	} else if (log_cond & DEEPIDLE_LOG_REDUCED) {
		/* Determine print SPM log or not */
		dpidle_log_print_curr_time = spm_get_current_time_ms();

		if (wakesta->assert_pc != 0)
			log_print = true;
#if 0
		/* Not wakeup by GPT */
		else if ((wakesta->r12 & (0x1 << 4)) == 0)
			log_print = true;
		else if (wakesta->timer_out <=
			 DPIDLE_LOG_PRINT_TIMEOUT_CRITERIA)
			log_print = true;
#endif
		else if ((dpidle_log_print_curr_time -
			  dpidle_log_print_prev_time) >
			 DPIDLE_LOG_DISCARD_CRITERIA)
			log_print = true;

		if (wakesta->timer_out <= DPIDLE_LOG_PRINT_TIMEOUT_CRITERIA)
			timer_out_too_short = true;

		/* Print SPM log */
		if (log_print == true) {
			dpidle_dbg(
	"dpidle_log_discard_cnt = %d, timer_out_too_short = %d, oper_cond = %x\n",
						dpidle_log_discard_cnt,
						timer_out_too_short,
						operation_cond);
			wr = __spm_output_wake_reason(wakesta, pcmdesc,
						      false, "dpidle");

			if (log_cond & DEEPIDLE_LOG_RESOURCE_USAGE)
				spm_resource_req_dump();

			dpidle_log_print_prev_time = dpidle_log_print_curr_time;
			dpidle_log_discard_cnt = 0;
			timer_out_too_short = false;
		} else {
			dpidle_log_discard_cnt++;

			wr = WR_NONE;
		}
	}

#ifdef CONFIG_MTK_ECCCI_DRIVER
	if (wakesta->r12 & WAKE_SRC_R12_MD2AP_PEER_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC, NULL, 0);
#endif

	return wr;
}


/*
 * dpidle_active_status() for pmic_throttling_dlpt
 * return 0 : entering dpidle recently ( > 1s) => normal mode(dlpt 10s)
 * return 1 : entering dpidle recently (<= 1s) =>
 *            light-loading mode(dlpt 20s)
 */
#define DPIDLE_ACTIVE_TIME		(1)
static struct timeval pre_dpidle_time;

int dpidle_active_status(void)
{
	struct timeval current_time;

	do_gettimeofday(&current_time);

	if ((current_time.tv_sec - pre_dpidle_time.tv_sec) > DPIDLE_ACTIVE_TIME)
		return 0;
	else if (((current_time.tv_sec - pre_dpidle_time.tv_sec) ==
		DPIDLE_ACTIVE_TIME) &&
		(current_time.tv_usec > pre_dpidle_time.tv_usec))
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL(dpidle_active_status);

unsigned int spm_go_to_dpidle(u32 spm_flags, u32 spm_data,
			      u32 log_cond, u32 operation_cond)
{
	struct wake_status wakesta;
	unsigned long flags;
#if defined(CONFIG_MTK_GIC_V3_EXT)
	struct mtk_irq_mask mask;
#endif
	unsigned int wr = WR_NONE;
	struct pcm_desc *pcmdesc = NULL;
	struct pwr_ctrl *pwrctrl = __spm_dpidle.pwrctrl;
	u32 cpu = smp_processor_id();
	u32 spm_flags1 = spm_data;

	cpu_footprint = cpu << CPU_FOOTPRINT_SHIFT;

	spm_dpidle_footprint(SPM_DEEPIDLE_ENTER);

	pwrctrl = __spm_dpidle.pwrctrl;

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	__sync_big_buck_ctrl_pcm_flag(&spm_flags1);
	__sync_vcore_ctrl_pcm_flag(operation_cond, &spm_flags1);
	set_pwrctrl_pcm_flags1(pwrctrl, spm_flags1);

	spin_lock_irqsave(&__spm_lock, flags);

	dpidle_profile_time(DPIDLE_PROFILE_NOTIFY_SSPM_BEFORE_WFI_START);

	spm_dpidle_notify_sspm_before_wfi(false, operation_cond, pwrctrl);

	dpidle_profile_time(DPIDLE_PROFILE_NOTIFY_SSPM_BEFORE_WFI_END);

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep_ex(SPM_IRQ0_ID);
	unmask_edge_trig_irqs_for_cirq();
#endif

#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_clone_gic();
	mt_cirq_enable();
#endif
	dpidle_profile_time(DPIDLE_PROFILE_CIRQ_ENABLE_END);

	spm_dpidle_pcm_setup_before_wfi(false, cpu, pcmdesc,
					pwrctrl, operation_cond);

	dpidle_profile_time(DPIDLE_PROFILE_SETUP_BEFORE_WFI_END);

	spm_dpidle_footprint(SPM_DEEPIDLE_ENTER_SSPM_ASYNC_IPI_BEFORE_WFI);

	dpidle_profile_time(
		DPIDLE_PROFILE_NOTIFY_SSPM_BEFORE_WFI_ASYNC_WAIT_START);

	spm_dpidle_notify_sspm_before_wfi_async_wait();

	dpidle_profile_time(
		DPIDLE_PROFILE_NOTIFY_SSPM_BEFORE_WFI_ASYNC_WAIT_END);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	/* Dump low power golden setting */
#if !defined(SPM_K414_EARLY_PORTING)
	if (operation_cond & DEEPIDLE_OPT_DUMP_LP_GOLDEN)
		mt_power_gs_dump_dpidle(GS_ALL);
#endif

	spm_dpidle_footprint(SPM_DEEPIDLE_ENTER_UART_SLEEP);

	if (!(operation_cond & DEEPIDLE_OPT_DUMP_LP_GOLDEN)) {
#if defined(CONFIG_MACH_MT6771)
		if (mtk8250_request_to_sleep()) {
#else
		if (request_uart_to_sleep()) {
#endif
			wr = WR_UART_BUSY;
			goto RESTORE_IRQ;
		}
	}
#endif

	dpidle_profile_time(DPIDLE_PROFILE_BEFORE_WFI);

	spm_dpidle_footprint(SPM_DEEPIDLE_ENTER_WFI);

	trace_dpidle_rcuidle(cpu, 1);

	spm_trigger_wfi_for_dpidle(pwrctrl);

	trace_dpidle_rcuidle(cpu, 0);

	dpidle_profile_time(DPIDLE_PROFILE_AFTER_WFI);

	spm_dpidle_footprint(SPM_DEEPIDLE_LEAVE_WFI);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (!(operation_cond & DEEPIDLE_OPT_DUMP_LP_GOLDEN))
#if defined(CONFIG_MACH_MT6771)
		mtk8250_request_to_wakeup();
#else
		request_uart_to_wakeup();
#endif
RESTORE_IRQ:
#endif

	dpidle_profile_time(DPIDLE_PROFILE_NOTIFY_SSPM_AFTER_WFI_START);

	spm_dpidle_notify_sspm_after_wfi(false, operation_cond);

	dpidle_profile_time(DPIDLE_PROFILE_NOTIFY_SSPM_AFTER_WFI_END);

	spm_dpidle_footprint(SPM_DEEPIDLE_LEAVE_SSPM_ASYNC_IPI_AFTER_WFI);

	__spm_get_wakeup_status(&wakesta);

	dpidle_profile_time(DPIDLE_PROFILE_SETUP_AFTER_WFI_START);

	spm_dpidle_pcm_setup_after_wfi(false, operation_cond);

	dpidle_profile_time(DPIDLE_PROFILE_SETUP_AFTER_WFI_END);

	spm_dpidle_footprint(SPM_DEEPIDLE_ENTER_UART_AWAKE);

	wr = spm_output_wake_reason(&wakesta, pcmdesc,
				    log_cond, operation_cond);

	dpidle_profile_time(DPIDLE_PROFILE_OUTPUT_WAKEUP_REASON_END);

#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_flush();
	mt_cirq_disable();
#endif

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_restore(&mask);
#endif

	dpidle_profile_time(DPIDLE_PROFILE_CIRQ_DISABLE_END);

	spin_unlock_irqrestore(&__spm_lock, flags);

	spm_dpidle_reset_footprint();

#if 1
	if (wr == WR_PCM_ASSERT)
		rekick_vcorefs_scenario();
#endif

	do_gettimeofday(&pre_dpidle_time);

	return wr;
}

/*
 * cpu_pdn:
 *    true  = CPU dormant
 *    false = CPU standby
 * pwrlevel:
 *    0 = AXI is off
 *    1 = AXI is 26M
 * pwake_time:
 *    >= 0  = specific wakeup period
 */
unsigned int spm_go_to_sleep_dpidle(u32 spm_flags, u32 spm_data)
{
	u32 sec = 0;
	u32 dpidle_timer_val = 0;
	u32 dpidle_wake_src = 0;
	struct wake_status wakesta;
	unsigned long flags;
#if defined(CONFIG_MTK_GIC_V3_EXT)
	struct mtk_irq_mask mask;
#endif
#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	struct wd_api *wd_api;
	int wd_ret;
#endif
	static unsigned int last_wr = WR_NONE;
	/* struct pcm_desc *pcmdesc = __spm_dpidle.pcmdesc; */
	struct pcm_desc *pcmdesc = NULL;
	struct pwr_ctrl *pwrctrl = __spm_dpidle.pwrctrl;
	int cpu = smp_processor_id();
	u32 spm_flags1 = spm_data;

	cpu_footprint = cpu << CPU_FOOTPRINT_SHIFT;

	spm_dpidle_footprint(SPM_DEEPIDLE_SLEEP_DPIDLE | SPM_DEEPIDLE_ENTER);

	pwrctrl = __spm_dpidle.pwrctrl;

	/* backup original dpidle setting */
	dpidle_timer_val = pwrctrl->timer_val;
	dpidle_wake_src = pwrctrl->wake_src;

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	__sync_big_buck_ctrl_pcm_flag(&spm_flags1);
	set_pwrctrl_pcm_flags1(pwrctrl, spm_flags1);


#if SPM_PWAKE_EN
	sec = _spm_get_wake_period(-1, last_wr);
#endif
	pwrctrl->timer_val = sec * 32768;

	pwrctrl->wake_src = spm_get_sleep_wakesrc();

#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	wd_ret = get_wd_api(&wd_api);
	if (!wd_ret) {
		wd_api->wd_spmwdt_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);
		wd_api->wd_suspend_notify();
	} else
		spm_crit2("FAILED TO GET WD API\n");
#endif

	lockdep_off();
	spin_lock_irqsave(&__spm_lock, flags);

	spm_dpidle_notify_sspm_before_wfi(true,
					  DEEPIDLE_OPT_VCORE_LP_MODE,
					  pwrctrl);

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_all(&mask);
	mt_irq_unmask_for_sleep_ex(SPM_IRQ0_ID);
	unmask_edge_trig_irqs_for_cirq();
#endif

#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_clone_gic();
	mt_cirq_enable();
#endif

	spm_crit2("sleep_deepidle, sec = %u, wakesrc = 0x%x [%u][%u]\n",
		sec, pwrctrl->wake_src,
		is_cpu_pdn(pwrctrl->pcm_flags),
		is_infra_pdn(pwrctrl->pcm_flags));

	spm_dpidle_pcm_setup_before_wfi(true, cpu, pcmdesc, pwrctrl, 0);

	spm_dpidle_footprint(SPM_DEEPIDLE_SLEEP_DPIDLE |
			     SPM_DEEPIDLE_ENTER_SSPM_ASYNC_IPI_BEFORE_WFI);

	spm_dpidle_notify_sspm_before_wfi_async_wait();

	spm_dpidle_footprint(SPM_DEEPIDLE_SLEEP_DPIDLE |
			     SPM_DEEPIDLE_ENTER_UART_SLEEP);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MACH_MT6771)
	if (mtk8250_request_to_sleep()) {
#else
	if (request_uart_to_sleep()) {
#endif
		last_wr = WR_UART_BUSY;
		goto RESTORE_IRQ;
	}
#endif

	spm_dpidle_footprint(SPM_DEEPIDLE_SLEEP_DPIDLE |
			     SPM_DEEPIDLE_ENTER_WFI);

	trace_dpidle_rcuidle(cpu, 1);

	spm_trigger_wfi_for_dpidle(pwrctrl);

	trace_dpidle_rcuidle(cpu, 0);

	spm_dpidle_footprint(SPM_DEEPIDLE_SLEEP_DPIDLE |
			     SPM_DEEPIDLE_LEAVE_WFI);

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#if defined(CONFIG_MACH_MT6771)
		mtk8250_request_to_wakeup();
#else
		request_uart_to_wakeup();
#endif
RESTORE_IRQ:
#endif

	spm_dpidle_notify_sspm_after_wfi(false, 0);

	spm_dpidle_footprint(SPM_DEEPIDLE_SLEEP_DPIDLE |
			     SPM_DEEPIDLE_LEAVE_SSPM_ASYNC_IPI_AFTER_WFI);

	__spm_get_wakeup_status(&wakesta);

	spm_dpidle_pcm_setup_after_wfi(true, 0);

	spm_dpidle_footprint(SPM_DEEPIDLE_SLEEP_DPIDLE |
			     SPM_DEEPIDLE_ENTER_UART_AWAKE);

	last_wr = __spm_output_wake_reason(&wakesta, pcmdesc,
					   true, "sleep_dpidle");

#if defined(CONFIG_MTK_SYS_CIRQ)
	mt_cirq_flush();
	mt_cirq_disable();
#endif

#if defined(CONFIG_MTK_GIC_V3_EXT)
	mt_irq_mask_restore(&mask);
#endif

	spin_unlock_irqrestore(&__spm_lock, flags);
	lockdep_on();

#if defined(CONFIG_MTK_WATCHDOG) && defined(CONFIG_MTK_WD_KICKER)
	if (!wd_ret) {
		if (!pwrctrl->wdt_disable)
			wd_api->wd_resume_notify();
		else
			spm_crit2("pwrctrl->wdt_disable %d\n",
				  pwrctrl->wdt_disable);
		wd_api->wd_spmwdt_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);
	}
#endif

	/* restore original dpidle setting */
	pwrctrl->timer_val = dpidle_timer_val;
	pwrctrl->wake_src = dpidle_wake_src;

	spm_dpidle_notify_sspm_after_wfi_async_wait();

	spm_dpidle_reset_footprint();

#if 1
	if (last_wr == WR_PCM_ASSERT)
		rekick_vcorefs_scenario();
#endif

	return last_wr;
}

void spm_deepidle_init(void)
{
	spm_deepidle_chip_init();
}

MODULE_DESCRIPTION("SPM-DPIdle Driver v0.1");
