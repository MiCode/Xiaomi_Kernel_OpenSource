// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/atomic.h>
#include <linux/cpuidle.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/cpumask.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/timekeeping.h>
#include <linux/rtc.h>
#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include <mt6885_spm_comm.h>

#include <mtk_lpm.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_call.h>
#include <mtk_lpm_type.h>
#include <mtk_lpm_call_type.h>
#include <mtk_dbg_common_v1.h>
#include <mt-plat/mtk_ccci_common.h>
#include <uapi/linux/sched/types.h>
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
#if BITS_PER_LONG == 64
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
#endif
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

static int __mt6885_suspend_prompt(int type, int cpu,
				   const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;
	unsigned int spm_res = 0;

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

PLAT_LEAVE_SUSPEND:
	return ret;
}

static void __mt6885_suspend_reflect(int type, int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	printk_deferred("[name:spm&][%s:%d] - prepare resume\n",
			__func__, __LINE__);

	mt6885_suspend_common_resume(mt6885_suspend_status);
	mt6885_do_mcusys_prepare_on();

	printk_deferred("[name:spm&][%s:%d] - resume\n",
			__func__, __LINE__);

	/* skip calling issuer when prepare fail*/
	if (mt6885_model_suspend.flag & MTK_LP_PREPARE_FAIL)
		return;

	if (issuer)
		issuer->log(MT_LPM_ISSUER_SUSPEND, "suspend", NULL);

	/* show md sleep duration during AP suspend */
	get_md_sleep_time(&after_md_sleep_status);
	log_md_sleep_info();
}
int mt6885_suspend_system_prompt(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	int is_resume_enter = 0;
#ifdef CONFIG_MTK_CCCI_DEVICES
	printk_deferred("[name:spm&][%s:%d] - notify MD that AP suspend\n",
		__func__, __LINE__);
	is_resume_enter = 1 << 0;
	exec_ccci_kern_func_by_md_id(MD_SYS1, ID_AP2MD_LOWPWR,
		(char *)&is_resume_enter, 4);
#endif

	return __mt6885_suspend_prompt(MTK_LPM_SUSPEND_S2IDLE,
				       cpu, issuer);
}

void mt6885_suspend_system_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	int is_resume_enter = 0;
#ifdef CONFIG_MTK_CCCI_DEVICES
	printk_deferred("[name:spm&][%s:%d] - notify MD that AP resume\n",
		__func__, __LINE__);
	is_resume_enter = 1 << 1;
	exec_ccci_kern_func_by_md_id(MD_SYS1, ID_AP2MD_LOWPWR,
		(char *)&is_resume_enter, 4);
#endif

	return __mt6885_suspend_reflect(MTK_LPM_SUSPEND_S2IDLE,
					cpu, issuer);
}

int mt6885_suspend_s2idle_prompt(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;

	cpumask_set_cpu(cpu, &s2idle_cpumask);
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {

#ifdef CONFIG_PM_SLEEP
		/* Notice
		 * Fix the rcu_idle workaround later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to using RCU_NONIDLE()
		 * with syscore. But anyway in s2idle, when lastest cpu
		 * enter idle state means there won't care r/w sync problem
		 * and RCU_NOIDLE maybe the right solution.
		 */
		RCU_NONIDLE({
			ret = syscore_suspend();
		});
#endif
		if (ret < 0)
			mt6885_model_suspend.flag |= MTK_LP_PREPARE_FAIL;

		ret = __mt6885_suspend_prompt(MTK_LPM_SUSPEND_S2IDLE,
					      cpu, issuer);
	}
	return ret;
}

int mt6885_suspend_s2idle_prepare_enter(int prompt, int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;

	if (mt6885_model_suspend.flag & MTK_LP_PREPARE_FAIL)
		ret = -1;

	return ret;
}

void mt6885_suspend_s2idle_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {
		__mt6885_suspend_reflect(MTK_LPM_SUSPEND_S2IDLE,
					 cpu, issuer);
#ifdef CONFIG_PM_SLEEP
		/* Notice
		 * Fix the rcu_idle/timekeeping workaround later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to using RCU_NONIDLE()
		 * with syscore.
		 */
		if (!(mt6885_model_suspend.flag & MTK_LP_PREPARE_FAIL))
			RCU_NONIDLE(syscore_resume());

		if (mt6885_model_suspend.flag & MTK_LP_PREPARE_FAIL)
			mt6885_model_suspend.flag &= (~MTK_LP_PREPARE_FAIL);

#endif
	}
	cpumask_clear_cpu(cpu, &s2idle_cpumask);
}

#define MT6885_SUSPEND_OP_INIT(_prompt, _enter, _resume, _reflect) ({\
	mt6885_model_suspend.op.prompt = _prompt;\
	mt6885_model_suspend.op.prepare_enter = _enter;\
	mt6885_model_suspend.op.prepare_resume = _resume;\
	mt6885_model_suspend.op.reflect = _reflect; })


struct mtk_lpm_model mt6885_model_suspend = {
	.flag = MTK_LP_REQ_NONE,
	.op = {
		.prompt = mt6885_suspend_system_prompt,
		.reflect = mt6885_suspend_system_reflect,
	}
};

static int mtk_lpm_suspend_prepare_late(void)
{
	int is_resume_enter = 0;
#ifdef CONFIG_MTK_CCCI_DEVICES
	printk_deferred("[name:spm&][%s:%d] - notify MD that AP suspend\n",
		__func__, __LINE__);
	is_resume_enter = 1 << 0;
	exec_ccci_kern_func_by_md_id(MD_SYS1, ID_AP2MD_LOWPWR,
		(char *)&is_resume_enter, 4);
#endif

	return 0;
}

static void mtk_lpm_suspend_restore(void)
{
	int is_resume_enter = 0;
#ifdef CONFIG_MTK_CCCI_DEVICES
	printk_deferred("[name:spm&][%s:%d] - notify MD that AP resume\n",
		__func__, __LINE__);
	is_resume_enter = 1 << 1;
	exec_ccci_kern_func_by_md_id(MD_SYS1, ID_AP2MD_LOWPWR,
		(char *)&is_resume_enter, 4);
#endif
}

static struct platform_s2idle_ops mtk_lpm_suspend_s2idle_ops = {
	.prepare = mtk_lpm_suspend_prepare_late,
	.restore = mtk_lpm_suspend_restore,
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
#endif /* CONFIG_PM */

int __init mt6885_model_suspend_init(void)
{
	int ret;

	int suspend_type = mtk_lpm_suspend_type_get();

	if (suspend_type == MTK_LPM_SUSPEND_S2IDLE) {
		MT6885_SUSPEND_OP_INIT(mt6885_suspend_s2idle_prompt,
					mt6885_suspend_s2idle_prepare_enter,
					NULL,
					mt6885_suspend_s2idle_reflect);
		mtk_lpm_suspend_registry("s2idle", &mt6885_model_suspend);
	} else {
		MT6885_SUSPEND_OP_INIT(mt6885_suspend_system_prompt,
					NULL,
					NULL,
					mt6885_suspend_system_reflect);
		mtk_lpm_suspend_registry("suspend", &mt6885_model_suspend);
	}

	cpumask_clear(&s2idle_cpumask);


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

	/* set s2idle ops */
	s2idle_set_ops(&mtk_lpm_suspend_s2idle_ops);

	return 0;
}
