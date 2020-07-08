// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpuidle.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/rtc.h>
#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include <mtk_lpm.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_call.h>
#include <mtk_lpm_type.h>
#include <mtk_lpm_call_type.h>
#include <mtk_dbg_common_v1.h>
//#include <mt-plat/mtk_ccci_common.h>

#include "mt6873.h"
#include "mt6873_suspend.h"

unsigned int mt6873_suspend_status;
u64 before_md_sleep_time;
u64 after_md_sleep_time;

void __attribute__((weak)) subsys_if_on(void)
{
	pr_info("[name:spm&]NO %s !!!\n", __func__);
}
void __attribute__((weak)) pll_if_on(void)
{
	pr_info("[name:spm&]NO %s !!!\n", __func__);
}
void __attribute__((weak)) gpio_dump_regs(void)
{
	pr_info("[name:spm&]NO %s !!!\n", __func__);
}

void mtk_suspend_gpio_dbg(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	gpio_dump_regs();
#endif
}
EXPORT_SYMBOL(mtk_suspend_gpio_dbg);

void mtk_suspend_clk_dbg(void){}
EXPORT_SYMBOL(mtk_suspend_clk_dbg);

#define MD_SLEEP_INFO_SMEM_OFFEST (4)
static u64 get_md_sleep_time(void)
{
	return 0;
}

static inline int mt6873_suspend_common_enter(unsigned int *susp_status)
{
	unsigned int status = PLAT_VCORE_LP_MODE
				| PLAT_PMIC_VCORE_SRCLKEN0
				| PLAT_SUSPEND;

	/* maybe need to stop sspm/mcupm mcdi task here */
	if (susp_status)
		*susp_status = status;

	return 0;
}


static inline int mt6873_suspend_common_resume(unsigned int susp_status)
{
	/* Implement suspend common flow here */
	return 0;
}

int mt6873_suspend_prompt(int cpu, const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;
	unsigned int spm_res = 0;
#ifdef CONFIG_MTK_CCCI_DEVICES
#if defined(CONFIG_ARM64)
	int len;
	int is_resume_enter = 0;
#endif
#endif
	mt6873_suspend_status = 0;

	ret = mt6873_suspend_common_enter(&mt6873_suspend_status);

	if (ret)
		goto PLAT_LEAVE_SUSPEND;

	/* Legacy SSPM flow, spm sw resource request flow */
	mt6873_do_mcusys_prepare_pdn(mt6873_suspend_status, &spm_res);

	/* Record md sleep time */
	before_md_sleep_time = get_md_sleep_time();

#ifdef CONFIG_MTK_CCCI_DEVICES
#if defined(CONFIG_ARM64)
	len = sizeof(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);
	if (strncmp(&CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES[len - 4],
		"_lp", 3) == 0) {
		is_resume_enter = 1 << 0;
		exec_ccci_kern_func_by_md_id(MD_SYS1, ID_AP2MD_LOWPWR,
			(char *)&is_resume_enter, 4);
	}
#endif
#endif

PLAT_LEAVE_SUSPEND:
	return ret;
}

void mt6873_suspend_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
#ifdef CONFIG_MTK_CCCI_DEVICES
#if defined(CONFIG_ARM64)
	int len;
	int is_resume_enter = 0;
#endif
#endif

#ifdef CONFIG_MTK_CCCI_DEVICES
#if defined(CONFIG_ARM64)
	len = sizeof(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES);
	if (strncmp(&CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES[len - 4],
		"_lp", 3) == 0) {
		is_resume_enter = 1 << 1;
		exec_ccci_kern_func_by_md_id(MD_SYS1, ID_AP2MD_LOWPWR,
			(char *)&is_resume_enter, 4);
	}
#endif
#endif
	mt6873_suspend_common_resume(mt6873_suspend_status);
	mt6873_do_mcusys_prepare_on();


	if (issuer)
		issuer->log(MT_LPM_ISSUER_SUSPEND, "suspend", NULL);

	/* show md sleep duration during AP suspend */
	after_md_sleep_time = get_md_sleep_time();
}

struct mtk_lpm_model mt6873_model_suspend = {
	.flag = MTK_LP_REQ_NONE,
	.op = {
		.prompt = mt6873_suspend_prompt,
		.reflect = mt6873_suspend_reflect,
	}
};

#ifdef CONFIG_PM
static int mt6873_spm_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block mt6873_spm_suspend_pm_notifier_func = {
	.notifier_call = mt6873_spm_suspend_pm_event,
	.priority = 0,
};
#endif

int __init mt6873_model_suspend_init(void)
{
	int ret;

	mtk_lpm_suspend_registry("suspend", &mt6873_model_suspend);

#ifdef CONFIG_PM
	ret = register_pm_notifier(&mt6873_spm_suspend_pm_notifier_func);
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register PM notifier.\n");
		return ret;
	}
#endif /* CONFIG_PM */

	return 0;
}
