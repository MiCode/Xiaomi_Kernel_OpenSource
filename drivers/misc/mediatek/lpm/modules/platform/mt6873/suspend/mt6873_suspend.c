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

#include "mt6873.h"
#include "mt6873_suspend.h"

unsigned int mt6873_suspend_status;
struct cpumask s2idle_cpumask;

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

static int __mt6873_suspend_prompt(int type, int cpu,
				   const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;
	unsigned int spm_res = 0;

	mt6873_suspend_status = 0;

	ret = mt6873_suspend_common_enter(&mt6873_suspend_status);

	if (ret)
		goto PLAT_LEAVE_SUSPEND;

	/* Legacy SSPM flow, spm sw resource request flow */
	mt6873_do_mcusys_prepare_pdn(mt6873_suspend_status, &spm_res);

PLAT_LEAVE_SUSPEND:
	return ret;
}

static void __mt6873_suspend_reflect(int type, int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	mt6873_suspend_common_resume(mt6873_suspend_status);
	mt6873_do_mcusys_prepare_on();

	if (issuer)
		issuer->log(MT_LPM_ISSUER_SUSPEND, "suspend", NULL);
}
int mt6873_suspend_system_prompt(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	return __mt6873_suspend_prompt(MTK_LPM_SUSPEND_S2IDLE,
				       cpu, issuer);
}

void mt6873_suspend_system_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	return __mt6873_suspend_reflect(MTK_LPM_SUSPEND_S2IDLE,
					cpu, issuer);
}

int mt6873_suspend_s2idle_prompt(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;

	cpumask_set_cpu(cpu, &s2idle_cpumask);
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {
#ifdef CONFIG_PM_SLEEP
		/* TODO
		 * Need to fix the rcu_idle workaround later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to using RCU_NONIDLE()
		 * with syscore. But anyway in s2idle, when lastest cpu
		 * enter idle state means there won't care r/w sync problem
		 * and RCU_NOIDLE maybe the right solution.
		 */
		syscore_suspend();
#endif
		ret = __mt6873_suspend_prompt(MTK_LPM_SUSPEND_S2IDLE,
					      cpu, issuer);
	}
	return ret;
}

void mt6873_suspend_s2idle_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {
		__mt6873_suspend_reflect(MTK_LPM_SUSPEND_S2IDLE,
					 cpu, issuer);
#ifdef CONFIG_PM_SLEEP
		/* TODO
		 * Need to fix the rcu_idle/timekeeping later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to using RCU_NONIDLE()
		 * with syscore.
		 */
		syscore_resume();
		pm_system_wakeup();
#endif
	}
	cpumask_clear_cpu(cpu, &s2idle_cpumask);
}

#define MT6873_SUSPEND_OP_INIT(_prompt, _enter, _resume, _reflect) ({\
	mt6873_model_suspend.op.prompt = _prompt;\
	mt6873_model_suspend.op.prepare_enter = _enter;\
	mt6873_model_suspend.op.prepare_resume = _resume;\
	mt6873_model_suspend.op.reflect = _reflect; })



struct mtk_lpm_model mt6873_model_suspend = {
	.flag = MTK_LP_REQ_NONE,
	.op = {
		.prompt = mt6873_suspend_system_prompt,
		.reflect = mt6873_suspend_system_reflect,
	}
};

#ifdef CONFIG_PM
static int mt6873_spm_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec64 ts;
	struct rtc_time tm;

	ktime_get_ts64(&ts);
	rtc_time64_to_tm(ts.tv_sec, &tm);

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

	int suspend_type = mtk_lpm_suspend_type_get();

	if (suspend_type == MTK_LPM_SUSPEND_S2IDLE) {
		MT6873_SUSPEND_OP_INIT(mt6873_suspend_s2idle_prompt,
					NULL,
					NULL,
					mt6873_suspend_s2idle_reflect);
		mtk_lpm_suspend_registry("s2idle", &mt6873_model_suspend);
	} else {
		MT6873_SUSPEND_OP_INIT(mt6873_suspend_system_prompt,
					NULL,
					NULL,
					mt6873_suspend_system_reflect);
		mtk_lpm_suspend_registry("suspend", &mt6873_model_suspend);
	}

	cpumask_clear(&s2idle_cpumask);

#ifdef CONFIG_PM
	ret = register_pm_notifier(&mt6873_spm_suspend_pm_notifier_func);
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register PM notifier.\n");
		return ret;
	}
#endif /* CONFIG_PM */

	return 0;
}
