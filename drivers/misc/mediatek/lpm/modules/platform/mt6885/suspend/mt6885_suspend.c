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
#include <mt-plat/mtk_ccci_common.h>

#include "mt6885.h"
#include "mt6885_suspend.h"

unsigned int mt6885_suspend_status;
struct md_sleep_status before_md_sleep_status;
struct md_sleep_status after_md_sleep_status;

struct cpumask s2idle_cpumask;
struct mtk_lpm_model mt6885_model_suspend;

void __attribute__((weak)) subsys_if_on(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}
void __attribute__((weak)) pll_if_on(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}
void __attribute__((weak)) gpio_dump_regs(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}

void mtk_suspend_gpio_dbg(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	gpio_dump_regs();
#endif
}
EXPORT_SYMBOL(mtk_suspend_gpio_dbg);

void mtk_suspend_clk_dbg(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	pll_if_on();
	subsys_if_on();
#endif /* CONFIG_FPGA_EARLY_PORTING */
}
EXPORT_SYMBOL(mtk_suspend_clk_dbg);

#define MD_SLEEP_INFO_SMEM_OFFEST (4)
static void get_md_sleep_time(struct md_sleep_status *md_data)
{
	/* dump subsystem sleep info */
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	u32 *share_mem = NULL;

	if (!md_data)
		return;

	share_mem = (u32 *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_LOW_POWER, NULL);
	if (share_mem == NULL) {
		printk_deferred("[name:spm&][%s:%d] - No MD share mem\n",
			 __func__, __LINE__);
		return;
	}
	share_mem = share_mem + MD_SLEEP_INFO_SMEM_OFFEST;
	memset(md_data, 0, sizeof(struct md_sleep_status));
	memcpy(md_data, share_mem, sizeof(struct md_sleep_status));
#else
	return;
#endif
}

static void log_md_sleep_info(void)
{
#define LOG_BUF_SIZE	256
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;

	if (after_md_sleep_status.sleep_time >= before_md_sleep_status.sleep_time) {
		printk_deferred("[name:spm&][SPM] md_slp_duration = %llu (32k)\n",
			after_md_sleep_status.sleep_time - before_md_sleep_status.sleep_time);

		log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size, "[name:spm&][SPM] ");
		log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size, "MD/2G/3G/4G/5G_FR1 = ");
		log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size, "%d.%03d/%d.%03d/%d.%03d/%d.%03d/%d.%03d seconds",
			(after_md_sleep_status.md_sleep_time -
				before_md_sleep_status.md_sleep_time) / 1000000,
			(after_md_sleep_status.md_sleep_time -
				before_md_sleep_status.md_sleep_time) % 1000000 / 1000,
			(after_md_sleep_status.gsm_sleep_time -
				before_md_sleep_status.gsm_sleep_time) / 1000000,
			(after_md_sleep_status.gsm_sleep_time -
				before_md_sleep_status.gsm_sleep_time) % 1000000 / 1000,
			(after_md_sleep_status.wcdma_sleep_time -
				before_md_sleep_status.wcdma_sleep_time) / 1000000,
			(after_md_sleep_status.wcdma_sleep_time -
				before_md_sleep_status.wcdma_sleep_time) % 1000000 / 1000,
			(after_md_sleep_status.lte_sleep_time -
				before_md_sleep_status.lte_sleep_time) / 1000000,
			(after_md_sleep_status.lte_sleep_time -
				before_md_sleep_status.lte_sleep_time) % 1000000 / 1000,
			(after_md_sleep_status.nr_sleep_time -
				before_md_sleep_status.nr_sleep_time) / 1000000,
			(after_md_sleep_status.nr_sleep_time -
				before_md_sleep_status.nr_sleep_time) % 10000000 / 1000);

		WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);
		printk_deferred("[name:spm&][SPM] %s", log_buf);
	}
}

static inline int mt6885_suspend_common_enter(unsigned int *susp_status)
{
	unsigned int status = PLAT_VCORE_LP_MODE
				| PLAT_PMIC_VCORE_SRCLKEN0
				| PLAT_SUSPEND;

	/* maybe need to stop sspm/mcupm mcdi task here */
	if (susp_status)
		*susp_status = status;

	return 0;
}


static inline int mt6885_suspend_common_resume(unsigned int susp_status)
{
	/* Implement suspend common flow here */
	return 0;
}

int mt6885_suspend_prompt(int cpu, const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;
	unsigned int spm_res = 0;
	int is_resume_enter = 0;

	mt6885_suspend_status = 0;

	printk_deferred("[name:spm&][%s:%d] - prepare suspend enter\n",
			__func__, __LINE__);

	ret = mt6885_suspend_common_enter(&mt6885_suspend_status);

	if (ret)
		goto PLAT_LEAVE_SUSPEND;

	/* Legacy SSPM flow, spm sw resource request flow */
	mt6885_do_mcusys_prepare_pdn(mt6885_suspend_status, &spm_res);

	printk_deferred("[name:spm&][%s:%d] - suspend enter\n",
			__func__, __LINE__);

	/* Record md sleep time */
	get_md_sleep_time(&before_md_sleep_status);

#ifdef CONFIG_MTK_CCCI_DEVICES
	printk_deferred("[name:spm&][%s:%d] - notify MD that AP suspend\n",
		__func__, __LINE__);
	is_resume_enter = 1 << 0;
	exec_ccci_kern_func_by_md_id(MD_SYS1, ID_AP2MD_LOWPWR,
			(char *)&is_resume_enter, 4);
#endif
PLAT_LEAVE_SUSPEND:
	return ret;
}

void mt6885_suspend_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	int is_resume_enter = 0;

	printk_deferred("[name:spm&][%s:%d] - prepare resume\n",
			__func__, __LINE__);

#ifdef CONFIG_MTK_CCCI_DEVICES
	printk_deferred("[name:spm&][%s:%d] - notify MD that AP resume\n",
		__func__, __LINE__);
	is_resume_enter = 1 << 1;
	exec_ccci_kern_func_by_md_id(MD_SYS1, ID_AP2MD_LOWPWR,
		(char *)&is_resume_enter, 4);
#endif
	mt6885_suspend_common_resume(mt6885_suspend_status);
	mt6885_do_mcusys_prepare_on();

	printk_deferred("[name:spm&][%s:%d] - resume\n",
			__func__, __LINE__);

	if (issuer)
		issuer->log(MT_LPM_ISSUER_SUSPEND, "suspend", NULL);

	/* show md sleep duration during AP suspend */
	get_md_sleep_time(&after_md_sleep_status);
	log_md_sleep_info();
}

struct mtk_lpm_model mt6885_model_suspend = {
	.flag = MTK_LP_REQ_NONE,
	.op = {
		.prompt = mt6885_suspend_prompt,
		.reflect = mt6885_suspend_reflect,
	}
};

#ifdef CONFIG_PM
static int mt6885_spm_suspend_pm_event(struct notifier_block *notifier,
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
		printk_deferred(
		"[name:spm&][SPM] PM: suspend entry %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		printk_deferred(
		"[name:spm&][SPM] PM: suspend exit %d-%02d-%02d %02d:%02d:%02d.%09lu UTC\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block mt6885_spm_suspend_pm_notifier_func = {
	.notifier_call = mt6885_spm_suspend_pm_event,
	.priority = 0,
};
#endif

int __init mt6885_model_suspend_init(void)
{
	int ret;

	mtk_lpm_suspend_registry("suspend", &mt6885_model_suspend);

#ifdef CONFIG_PM
	ret = register_pm_notifier(&mt6885_spm_suspend_pm_notifier_func);
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register PM notifier.\n");
		return ret;
	}
#endif /* CONFIG_PM */

#ifdef CONFIG_PM_SLEEP_DEBUG
	pm_print_times_enabled = false;
#endif
	return 0;
}
