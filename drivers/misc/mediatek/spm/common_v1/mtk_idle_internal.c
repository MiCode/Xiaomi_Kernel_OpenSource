// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/ratelimit.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>

//#include <mtk_power_gs_api.h>

#if IS_ENABLED(CONFIG_THERMAL)
//#include <mtk_thermal.h> /* mtkTTimer_start/cancel_timer */
#endif

//#include <mtk_clkbuf_ctl.h> /* clk_buf_bblpm_enter_cond */

#include <mtk_idle.h>
#include <mtk_idle_internal.h>

#include <mtk_spm_internal.h>
#if IS_ENABLED(CONFIG_MTK_UFS_SUPPORT)
#include <ufs-mtk.h>
#endif
//#include <mt-plat/mtk_boot.h>

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
EXPORT_SYMBOL(mtk_idle_block_reason_name);

unsigned int ufs_cb_before_idle(void)
{
	unsigned int op_cond = 0;
#if IS_ENABLED(CONFIG_MTK_UFS_SUPPORT)
	int boot_type;

	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_UFS) {
		op_cond |=
			!ufs_mtk_deepidle_hibern8_check()
					? MTK_IDLE_OPT_XO_UFS_ON_OFF : 0;
	}
#endif

	#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
	op_cond |= !clk_buf_bblpm_enter_cond()
				? MTK_IDLE_OPT_CLKBUF_BBLPM : 0;
	#endif

	return op_cond;
}

void ufs_cb_after_idle(void)
{
#if IS_ENABLED(CONFIG_MTK_UFS_SUPPORT)
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
	#if IS_ENABLED(CONFIG_KALLSYMS)
	char namebuf[128] = {0};

	sprint_symbol(namebuf, (unsigned long)n->notifier_call);
	pr_info("Power/swap [mt_idle_ntf] <%02d>%08lx (%s)\n",
		index++, (unsigned long)n->notifier_call, namebuf);
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
 *		      => normal mode(dlpt 10s)
 *   return 1 : entering  recently (<= 1s)
 *		      => light-loading mode(dlpt 20s)
 */
#define MTK_IDLE_ACTIVE_TIME		(1)
struct timespec64 pre_idle_rec_time;
bool is_mtk_idle_active(void)
{
	struct timespec64 current_time;
	long diff;

	ktime_get_real_ts64(&current_time);
	//tv->tv_sec = now.tv_sec;
	//tv->tv_usec = now.tv_nsec/1000;

	//do_gettimeofday(&current_time);
	diff = current_time.tv_sec - pre_idle_rec_time.tv_sec;

	if (diff > MTK_IDLE_ACTIVE_TIME)
		return false;
	else if ((diff == MTK_IDLE_ACTIVE_TIME) &&
		(current_time.tv_nsec > pre_idle_rec_time.tv_nsec))
		return false;
	else
		return true;
}
EXPORT_SYMBOL(is_mtk_idle_active);

void mtk_idle_update_time(int IdleModel)
{
	(void)IdleModel;
	ktime_get_real_ts64(&pre_idle_rec_time);
}

MODULE_LICENSE("GPL");
