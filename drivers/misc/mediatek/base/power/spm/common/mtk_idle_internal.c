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
#include <linux/ratelimit.h>
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
#if defined(CONFIG_MTK_UFS_SUPPORT)
#include <ufs-mtk.h>
#endif
#include <mt-plat/mtk_boot.h>

#include <mtk_idle_module.h>
/* External weak functions: implemented in clkbuf and thermal module */
uint32_t __attribute__((weak)) clk_buf_bblpm_enter_cond(void) { return -1; }

/***********************************************************
 * local functions
 ***********************************************************/

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
		reason == BY_PWM ? GET_ENUM_STRING(BY_PWM) :
		reason == BY_PLL ? GET_ENUM_STRING(BY_PLL) : "null";
}

unsigned int ufs_cb_before_idle(void)
{
	unsigned int op_cond = 0;
#if defined(CONFIG_MTK_UFS_SUPPORT)
	int boot_type;

	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_UFS) {
		op_cond |=
			!ufs_mtk_deepidle_hibern8_check()
					? MTK_IDLE_OPT_XO_UFS_ON_OFF : 0;
	}
#endif

	#if !defined(CONFIG_FPGA_EARLY_PORTING)
	op_cond |= !clk_buf_bblpm_enter_cond()
				? MTK_IDLE_OPT_CLKBUF_BBLPM : 0;
	#endif

	return op_cond;
}

void ufs_cb_after_idle(void)
{
#if defined(CONFIG_MTK_UFS_SUPPORT)
	int boot_type;

	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_UFS)
		ufs_mtk_deepidle_leave();
#endif
}



/*****************************************************
 *  mtk idle notification
 *****************************************************/
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

	ret = mtk_idle_module_notifier_register(n);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_register);

void mtk_idle_notifier_unregister(struct notifier_block *n)
{
	mtk_idle_module_notifier_unregister(n);
}
EXPORT_SYMBOL_GPL(mtk_idle_notifier_unregister);


int mtk_idle_model_enter(int cpu, int IdleModelType)
{
	int bRet = 0;

	bRet = mtk_idle_module_model_enter(IdleModelType, cpu);

	if (bRet != MTK_IDLE_MOD_OK)
		bRet = -1;
	else
		bRet = 0;

	return bRet;
}

/* mtk_dpidle_is_active() for pmic_throttling_dlpt
 *   return 0 : entering  recently ( > 1s)
 *                      => normal mode(dlpt 10s)
 *   return 1 : entering  recently (<= 1s)
 *                      => light-loading mode(dlpt 20s)
 */
#define MTK_IDLE_ACTIVE_TIME		(1)
struct timeval pre_idle_rec_time;
bool is_mtk_idle_active(void)
{
	struct timeval current_time;
	long int diff;

	do_gettimeofday(&current_time);
	diff = current_time.tv_sec - pre_idle_rec_time.tv_sec;

	if (diff > MTK_IDLE_ACTIVE_TIME)
		return false;
	else if ((diff == MTK_IDLE_ACTIVE_TIME) &&
		(current_time.tv_usec > pre_idle_rec_time.tv_usec))
		return false;
	else
		return true;
}
EXPORT_SYMBOL(is_mtk_idle_active);

void mtk_idle_update_time(int IdleModel)
{
	(void)IdleModel;
	do_gettimeofday(&pre_idle_rec_time);
}

