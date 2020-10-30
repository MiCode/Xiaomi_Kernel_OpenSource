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
#include <linux/rcupdate.h>

#include <lpm.h>
#include <lpm_module.h>
#include <lpm_call.h>
#include <lpm_type.h>
#include <lpm_call_type.h>
#include <lpm_dbg_common_v1.h>

#include "lpm_plat.h"
#include "lpm_plat_comm.h"
#include "lpm_plat_suspend.h"

unsigned int lpm_suspend_status;
struct cpumask s2idle_cpumask;

static inline int lpm_suspend_common_enter(unsigned int *susp_status)
{
	unsigned int status = PLAT_VCORE_LP_MODE
				| PLAT_PMIC_VCORE_SRCLKEN0
				| PLAT_SUSPEND;

	/* maybe need to stop sspm/mcupm mcdi task here */
	if (susp_status)
		*susp_status = status;

	return 0;
}


static inline int lpm_suspend_common_resume(unsigned int susp_status)
{
	/* Implement suspend common flow here */
	return 0;
}

static int __lpm_suspend_prompt(int type, int cpu,
				   const struct lpm_issuer *issuer)
{
	int ret = 0;
	unsigned int spm_res = 0;

	lpm_suspend_status = 0;

	ret = lpm_suspend_common_enter(&lpm_suspend_status);

	if (ret)
		goto PLAT_LEAVE_SUSPEND;

	/* Legacy SSPM flow, spm sw resource request flow */
	lpm_do_mcusys_prepare_pdn(lpm_suspend_status, &spm_res);

PLAT_LEAVE_SUSPEND:
	return ret;
}

static void __lpm_suspend_reflect(int type, int cpu,
					const struct lpm_issuer *issuer)
{
	lpm_suspend_common_resume(lpm_suspend_status);
	lpm_do_mcusys_prepare_on();

	if (issuer)
		issuer->log(LPM_ISSUER_SUSPEND, "suspend", NULL);
}
int lpm_suspend_system_prompt(int cpu,
					const struct lpm_issuer *issuer)
{
	return __lpm_suspend_prompt(LPM_SUSPEND_S2IDLE,
				       cpu, issuer);
}

void lpm_suspend_system_reflect(int cpu,
					const struct lpm_issuer *issuer)
{
	return __lpm_suspend_reflect(LPM_SUSPEND_S2IDLE,
					cpu, issuer);
}

int lpm_suspend_s2idle_prompt(int cpu,
					const struct lpm_issuer *issuer)
{
	int ret = 0;

	cpumask_set_cpu(cpu, &s2idle_cpumask);
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {
		rcu_idle_exit();
#if IS_ENABLED(CONFIG_PM_SLEEP)
		/* TODO
		 * Need to fix the rcu_idle workaround later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to use rcu_idle_exit to
		 * wake up RCU, or using RCU_NONIDLE() with syscore.
		 * But anyway in s2idle, when lastest cpu
		 * enter idle state means there won't care r/w sync problem
		 * and RCU_NONIDLE() maybe the right solution.
		 */
		syscore_suspend();
#endif
		ret = __lpm_suspend_prompt(LPM_SUSPEND_S2IDLE,
					      cpu, issuer);
		rcu_idle_enter();
	}
	return ret;
}

void lpm_suspend_s2idle_reflect(int cpu,
					const struct lpm_issuer *issuer)
{
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {
		rcu_idle_exit();
		__lpm_suspend_reflect(LPM_SUSPEND_S2IDLE,
					 cpu, issuer);
#if IS_ENABLED(CONFIG_PM_SLEEP)
		/* TODO
		 * Need to fix the rcu_idle/timekeeping later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to use rcu_idle_exit to
		 * wake up RCU, or using RCU_NONIDLE() with syscore.
		 * But anyway in s2idle, when lastest cpu
		 * enter idle state means there won't care r/w sync problem
		 * and RCU_NONIDLE() maybe the right solution.
		 */
		syscore_resume();
		pm_system_wakeup();
#endif
		rcu_idle_enter();
	}
	cpumask_clear_cpu(cpu, &s2idle_cpumask);
}

#define LPM_SUSPEND_OP_INIT(_prompt, _enter, _resume, _reflect) ({\
	lpm_model_suspend.op.prompt = _prompt;\
	lpm_model_suspend.op.prepare_enter = _enter;\
	lpm_model_suspend.op.prepare_resume = _resume;\
	lpm_model_suspend.op.reflect = _reflect; })



struct lpm_model lpm_model_suspend = {
	.flag = LPM_REQ_NONE,
	.op = {
		.prompt = lpm_suspend_system_prompt,
		.reflect = lpm_suspend_system_reflect,
	}
};

#if IS_ENABLED(CONFIG_PM)
static int lpm_spm_suspend_pm_event(struct notifier_block *notifier,
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

static struct notifier_block lpm_spm_suspend_pm_notifier_func = {
	.notifier_call = lpm_spm_suspend_pm_event,
	.priority = 0,
};
#endif

int __init lpm_model_suspend_init(void)
{
	int ret;

	int suspend_type = lpm_suspend_type_get();

	if (suspend_type == LPM_SUSPEND_S2IDLE) {
		LPM_SUSPEND_OP_INIT(lpm_suspend_s2idle_prompt,
					NULL,
					NULL,
					lpm_suspend_s2idle_reflect);
		lpm_suspend_registry("s2idle", &lpm_model_suspend);
	} else {
		LPM_SUSPEND_OP_INIT(lpm_suspend_system_prompt,
					NULL,
					NULL,
					lpm_suspend_system_reflect);
		lpm_suspend_registry("suspend", &lpm_model_suspend);
	}

	cpumask_clear(&s2idle_cpumask);

#if IS_ENABLED(CONFIG_PM)
	ret = register_pm_notifier(&lpm_spm_suspend_pm_notifier_func);
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register PM notifier.\n");
		return ret;
	}
#endif /* CONFIG_PM */

	return 0;
}
