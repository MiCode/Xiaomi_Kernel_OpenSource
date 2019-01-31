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
#include <linux/kallsyms.h>

#if 0 /* FIXME: Golden setting dump not ready */
#include <mtk_power_gs_api.h>
#endif
#if defined(CONFIG_THERMAL)
#include <mtk_thermal.h> /* mtkTTimer_start/cancel_timer */
#endif
#if 0 /* FIXME: clkbuf is not ready */
#include <mtk_clkbuf_ctl.h> /* clk_buf_bblpm_enter_cond */
#endif

#include <mtk_idle.h>
#include <mtk_idle_internal.h>

#include <mtk_spm_internal.h>


/* --------------------------------------
 *   mtk idle scenario footprint definitions
 *  --------------------------------------
 */

enum idle_fp_step {
	IDLE_FP_ENTER = 0x1,
	IDLE_FP_PREHANDLER = 0x3,
	IDLE_FP_PCM_SETUP = 0x7,
	IDLE_FP_PWR_PRE_SYNC = 0xf,
	IDLE_FP_UART_SLEEP = 0x1f,
	IDLE_FP_ENTER_WFI = 0xff,
	IDLE_FP_LEAVE_WFI = 0x1ff,
	IDLE_FP_UART_RESUME = 0x3ff,
	IDLE_FP_PCM_CLEANUP = 0x7ff,
	IDLE_FP_POSTHANDLER = 0xfff,
	IDLE_FP_PWR_POST_SYNC = 0x1fff,
	IDLE_FP_LEAVE = 0xffff,
	IDLE_FP_SLEEP_DEEPIDLE = 0x80000000,
};

#ifdef CONFIG_MTK_RAM_CONSOLE
typedef void (*idle_footprint_t) (u32 val);

static idle_footprint_t fp[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] = aee_rr_rec_deepidle_val,
	[IDLE_TYPE_SO3] = aee_rr_rec_sodi3_val,
	[IDLE_TYPE_SO] = aee_rr_rec_sodi_val,
};
#define __mtk_idle_footprint(value)	\
	fp[idle_type]( \
		((op_cond & MTK_IDLE_OPT_SLEEP_DPIDLE) \
					? IDLE_FP_SLEEP_DEEPIDLE : 0) | \
		(smp_processor_id() << 24) | value)
#define __mtk_idle_footprint_reset(idle_type) \
	fp[idle_type](0)
#else /* CONFIG_MTK_RAM_CONSOLE */
#define __mtk_idle_footprint(value)
#define __mtk_idle_footprint_reset(idle_type)
#endif /* CONFIG_MTK_RAM_CONSOLE */


/************************************************************
 * Weak functions for chip dependent flow.
 ************************************************************/
/* [ByChip] Internal weak functions: implemented in mtk_spm_idle.c */
int __attribute__((weak)) mtk_idle_trigger_wfi(
	int idle_type, unsigned int idle_flag, int cpu)
{
	pr_notice("Power/swap %s is not implemented!\n", __func__);

	do {
		isb();
		mb();	/* memory barrier */
		__asm__ __volatile__("wfi" : : : "memory");
	} while (0);

	return 0;
}

void __attribute__((weak)) mtk_idle_pre_process_by_chip(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag) {}

void __attribute__((weak)) mtk_idle_post_process_by_chip(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag) {}

bool __attribute__((weak)) mtk_idle_cond_vcore_lp_mode(int idle_type)
{
	return false;
}

/* [ByChip] internal weak functions: implmented in mtk_spm_power.c */
void __attribute__((weak)) mtk_idle_power_pre_process(
	int idle_type, unsigned int op_cond) {}

void __attribute__((weak)) mtk_idle_power_pre_process_async_wait(
	int idle_type, unsigned int op_cond) {}

void __attribute__((weak)) mtk_idle_power_post_process(
	int idle_type, unsigned int op_cond) {}

void __attribute__((weak)) mtk_idle_power_post_process_async_wait(
	int idle_type, unsigned int op_cond) {}


/* External weak functions: implemented in clkbuf and thermal module */
uint32_t __attribute__((weak)) clk_buf_bblpm_enter_cond(void) { return -1; }

/***********************************************************
 * local functions
 ***********************************************************/

/* local definitions */
static void mtk_idle_notifier_call_chain(unsigned long val);

const char*
	mtk_idle_block_reason_name(int reason)
{
	#define GET_ENUM_STRING(str)	#str
	return reason == BY_FRM ? GET_ENUM_STRING(BY_FRM) :
		reason == BY_SRR ? GET_ENUM_STRING(BY_SSR) :
		reason == BY_UFS ? GET_ENUM_STRING(BY_UFS) :
		reason == BY_TEE ? GET_ENUM_STRING(BY_TEE) :
		reason == BY_DCS ? GET_ENUM_STRING(BY_DCS) :
		reason == BY_CLK ? GET_ENUM_STRING(BY_CLK) :
		reason == BY_DIS ? GET_ENUM_STRING(BY_DIS) :
		reason == BY_PWM ? GET_ENUM_STRING(BY_PWM) :
		reason == BY_PLL ? GET_ENUM_STRING(BY_PLL) :
		reason == BY_BOOT ? GET_ENUM_STRING(BY_BOOT) : "null";
}

static unsigned int ufs_cb_before_idle(void)
{
	unsigned int op_cond = 0;

	#if defined(CONFIG_MTK_UFS_BOOTING)
	op_cond |=
		!ufs_mtk_deepidle_hibern8_check()
				? MTK_IDLE_OPT_XO_UFS_ON_OFF : 0;
	#endif

	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	op_cond |= !clk_buf_bblpm_enter_cond()
				? MTK_IDLE_OPT_CLKBUF_BBLPM : 0;
	#endif

	return op_cond;
}

static void ufs_cb_after_idle(void)
{
	#if defined(CONFIG_MTK_UFS_BOOTING)
	ufs_mtk_deepidle_leave();
	#endif
}

static int idle_notify_enter[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] = NOTIFY_DPIDLE_ENTER,
	[IDLE_TYPE_SO3] = NOTIFY_SOIDLE3_ENTER,
	[IDLE_TYPE_SO] = NOTIFY_SOIDLE_ENTER,
};

static int idle_notify_leave[NR_IDLE_TYPES] = {
	[IDLE_TYPE_DP] = NOTIFY_DPIDLE_LEAVE,
	[IDLE_TYPE_SO3] = NOTIFY_SOIDLE3_LEAVE,
	[IDLE_TYPE_SO] = NOTIFY_SOIDLE_LEAVE,
};

static unsigned int mtk_idle_pre_handler(int idle_type)
{
	unsigned int op_cond = 0;

	/* notify mtk idle enter */
	mtk_idle_notifier_call_chain(idle_notify_enter[idle_type]);

	#if defined(CONFIG_THERMAL) && !defined(CONFIG_FPGA_EARLY_PORTING)
	/* cancel thermal hrtimer for power saving */
	mtkTTimer_cancel_timer();
	#endif

	/* check ufs */
	op_cond |= ufs_cb_before_idle();

	/* check vcore voltage config */
	op_cond |= (mtk_idle_cond_vcore_lp_mode(idle_type) ?
		MTK_IDLE_OPT_VCORE_LP_MODE : 0);

	return op_cond;
}

static void mtk_idle_post_handler(int idle_type)
{
	#if defined(CONFIG_THERMAL) && !defined(CONFIG_FPGA_EARLY_PORTING)
	/* restart thermal hrtimer for update temp info */
	mtkTTimer_start_timer();
	#endif

	ufs_cb_after_idle();

	/* notify mtk idle leave */
	mtk_idle_notifier_call_chain(idle_notify_leave[idle_type]);
}

/************************************************************
 * mtk idle flow for dp/so3/so
 ************************************************************/

int mtk_idle_enter(
	int idle_type, int cpu, unsigned int op_cond, unsigned int idle_flag)
{
	if (idle_type != IDLE_TYPE_DP &&
		idle_type != IDLE_TYPE_SO3 &&
		idle_type != IDLE_TYPE_SO)
		return -1;

	/* Disable log when we profiling idle latency */
	if (mtk_idle_latency_profile_is_on())
		idle_flag |= MTK_IDLE_LOG_DISABLE;

	__mtk_idle_footprint(IDLE_FP_ENTER);

	__profile_idle_stop(idle_type, PIDX_SELECT_TO_ENTER);

	__profile_idle_start(idle_type, PIDX_ENTER_TOTAL);

	/* idle pre handler: setup notification/thermal/ufs */
	__profile_idle_start(idle_type, PIDX_PRE_HANDLER);
	if (!(op_cond & MTK_IDLE_OPT_SLEEP_DPIDLE))
		op_cond |= mtk_idle_pre_handler(idle_type);
	__profile_idle_stop(idle_type, PIDX_PRE_HANDLER);

	__mtk_idle_footprint(IDLE_FP_PREHANDLER);

	/* [by_chip] pre power setting: setup sleep voltage and power mode */
	__profile_idle_start(idle_type, PIDX_PWR_PRE_WFI);
	mtk_idle_power_pre_process(idle_type, op_cond);
	__profile_idle_stop(idle_type, PIDX_PWR_PRE_WFI);

	/* [by_chip] spm setup */
	__profile_idle_start(idle_type, PIDX_SPM_PRE_WFI);
	mtk_idle_pre_process_by_chip(idle_type, cpu, op_cond, idle_flag);
	__profile_idle_stop(idle_type, PIDX_SPM_PRE_WFI);

	__mtk_idle_footprint(IDLE_FP_PCM_SETUP);

	/* [by_chip] pre power setting sync wait */
	__profile_idle_start(idle_type, PIDX_PWR_PRE_WFI_WAIT);
	mtk_idle_power_pre_process_async_wait(idle_type, op_cond);
	__profile_idle_stop(idle_type, PIDX_PWR_PRE_WFI_WAIT);

	__mtk_idle_footprint(IDLE_FP_PWR_PRE_SYNC);

	__mtk_idle_footprint(IDLE_FP_UART_SLEEP);

	/* uart sleep */
	#if defined(CONFIG_SERIAL_8250_MT6577)
	if (!(idle_flag & MTK_IDLE_LOG_DUMP_LP_GS)) {
		if (mtk8250_request_to_sleep()) {
			pr_notice("Power/swap Fail to request uart sleep\n");
			goto RESTORE_UART;
		}
	}
	#endif

	__mtk_idle_footprint(IDLE_FP_ENTER_WFI);

	/* [by_chip] enter cpuidle driver for wfi */
	__profile_idle_stop(idle_type, PIDX_ENTER_TOTAL);
	mtk_idle_trigger_wfi(idle_type, idle_flag, cpu);
	__profile_idle_start(idle_type, PIDX_LEAVE_TOTAL);

	__mtk_idle_footprint(IDLE_FP_LEAVE_WFI);

	/* uart resume */
	#if defined(CONFIG_SERIAL_8250_MT6577)
	if (!(idle_flag & MTK_IDLE_LOG_DUMP_LP_GS))
		mtk8250_request_to_wakeup();
RESTORE_UART:
	#endif

	__mtk_idle_footprint(IDLE_FP_UART_RESUME);

	/* [by_chip] post power setting: restore  */
	__profile_idle_start(idle_type, PIDX_PWR_POST_WFI);
	mtk_idle_power_post_process(idle_type, op_cond);
	__profile_idle_stop(idle_type, PIDX_PWR_POST_WFI);

	/* [by_chip] spm clean up */
	__profile_idle_start(idle_type, PIDX_SPM_POST_WFI);
	mtk_idle_post_process_by_chip(idle_type, cpu, op_cond, idle_flag);
	__profile_idle_stop(idle_type, PIDX_SPM_POST_WFI);

	__mtk_idle_footprint(IDLE_FP_PCM_CLEANUP);

	/* idle post handler: setup notification/thermal/ufs */
	__profile_idle_start(idle_type, PIDX_POST_HANDLER);
	if (!(op_cond & MTK_IDLE_OPT_SLEEP_DPIDLE))
		mtk_idle_post_handler(idle_type);
	__profile_idle_stop(idle_type, PIDX_POST_HANDLER);

	__mtk_idle_footprint(IDLE_FP_POSTHANDLER);

	/* [by_chip] post power setting sync wait */
	__profile_idle_start(idle_type, PIDX_PWR_POST_WFI_WAIT);
	mtk_idle_power_post_process_async_wait(idle_type, op_cond);
	__profile_idle_stop(idle_type, PIDX_PWR_POST_WFI_WAIT);

	__mtk_idle_footprint(IDLE_FP_PWR_POST_SYNC);

	__profile_idle_stop(idle_type, PIDX_LEAVE_TOTAL);

	__mtk_idle_footprint(IDLE_FP_LEAVE);

	/* output idle latency profiling result if enabled */
	mtk_idle_latency_profile_result(idle_type);

	__mtk_idle_footprint_reset(idle_type);

	return 0;
}


/*****************************************************
 *  mtk idle notification
 *****************************************************/

/* mtk_idle_notifier */
static RAW_NOTIFIER_HEAD(mtk_idle_notifier);

static void mtk_idle_notifier_call_chain(unsigned long val)
{
	raw_notifier_call_chain(&mtk_idle_notifier, val, NULL);
}

int mtk_idle_notifier_register(struct notifier_block *n)
{
	int ret = 0;
	int index = 0;
	#ifdef CONFIG_KALLSYMS
	char namebuf[128] = {0};
	const char *symname = NULL;

	symname = kallsyms_lookup((unsigned long)n->notifier_call,
			NULL, NULL, NULL, namebuf);
	if (symname) {
		pr_info("Power/swap [mt_idle_ntf] <%02d>%08lx (%s)\n",
			index++, (unsigned long)n->notifier_call, symname);
	} else {
		pr_info("Power/swap [mt_idle_ntf] <%02d>%08lx\n",
			index++, (unsigned long)n->notifier_call);
	}
	#else
	pr_info("Power/swap [mt_idle_ntf] <%02d>%08lx\n",
			index++, (unsigned long)n->notifier_call);
	#endif

	ret = raw_notifier_chain_register(&mtk_idle_notifier, n);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_register);

void mtk_idle_notifier_unregister(struct notifier_block *n)
{
	raw_notifier_chain_unregister(&mtk_idle_notifier, n);
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_unregister);

