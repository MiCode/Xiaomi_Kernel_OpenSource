// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpuidle.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <asm/cpuidle.h>
#include <asm/suspend.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/sched/signal.h>
#include <linux/spinlock.h>
#include <linux/cpuidle.h>
#include <linux/pm_qos.h>
#include <uapi/linux/sched/types.h>

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
static struct pm_qos_request lpm_qos_request;

#define S2IDLE_STATE_NAME "s2idle"

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
	struct lpm_issuer _suspend_issuer;

	lpm_suspend_common_resume(lpm_suspend_status);
	lpm_do_mcusys_prepare_on();
	if (issuer) {
		/* make sure suspend always print log */
		_suspend_issuer.log_type = LOG_SUCCEESS;
		_suspend_issuer.log = issuer->log;
		_suspend_issuer.log(LPM_ISSUER_SUSPEND, "suspend", (void *)&_suspend_issuer);
	}
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

		pr_info("[name:spm&][%s:%d] - suspend enter\n",
			__func__, __LINE__);

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
		/* Record md sleep status*/
		get_md_sleep_time(&before_md_sleep_status);
#endif
		ret = __lpm_suspend_prompt(LPM_SUSPEND_S2IDLE,
					      cpu, issuer);
	}
	return ret;
}

void lpm_suspend_s2idle_reflect(int cpu,
					const struct lpm_issuer *issuer)
{
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
		/* show md sleep status */
		get_md_sleep_time(&after_md_sleep_status);
#endif
		__lpm_suspend_reflect(LPM_SUSPEND_S2IDLE,
					 cpu, issuer);
		pr_info("[name:spm&][%s:%d] - resume\n",
			__func__, __LINE__);

		pm_system_wakeup();
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
	struct rtc_time tm;
	struct timespec64 tv = { 0 };
	/* android time */
	struct rtc_time tm_android;
	struct timespec64 tv_android = { 0 };

	ktime_get_real_ts64(&tv);
	tv_android = tv;
	rtc_time64_to_tm(tv.tv_sec, &tm);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE:
		pr_info("[name:spm&][SPM] suspend start %d-%02d-%02d %02d:%02d:%02d.%u UTC;"
			"android time %d-%02d-%02d %02d:%02d:%02d.%03d\n",
			tm.tm_year + 1900, tm.tm_mon + 1,
			tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned int)(tv.tv_nsec / 1000), tm_android.tm_year + 1900,
			tm_android.tm_mon + 1, tm_android.tm_mday, tm_android.tm_hour,
			tm_android.tm_min, tm_android.tm_sec,
			(unsigned int)(tv_android.tv_nsec / 1000));
		cpu_hotplug_disable();
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		pr_info("[name:spm&][SPM] suspend end %d-%02d-%02d %02d:%02d:%02d.%u UTC;"
			"android time %d-%02d-%02d %02d:%02d:%02d.%03d\n",
			tm.tm_year + 1900, tm.tm_mon + 1,
			tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned int)(tv.tv_nsec / 1000), tm_android.tm_year + 1900,
			tm_android.tm_mon + 1, tm_android.tm_mday, tm_android.tm_hour,
			tm_android.tm_min, tm_android.tm_sec,
			(unsigned int)(tv_android.tv_nsec / 1000));
		cpu_hotplug_enable();
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block lpm_spm_suspend_pm_notifier_func = {
	.notifier_call = lpm_spm_suspend_pm_event,
	.priority = 0,
};

#define MTK_LPM_SLEEP_COMPATIBLE_STRING "mediatek,sleep"
static int spm_irq_number = -1;
static irqreturn_t spm_irq_handler(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}

static inline unsigned int virq_to_hwirq(unsigned int virq)
{
	struct irq_desc *desc;
	unsigned int hwirq;

	desc = irq_to_desc(virq);
	WARN_ON(!desc);
	hwirq = desc ? desc->irq_data.hwirq : 0;
	return hwirq;
}

static int lpm_init_spm_irq(void)
{
	struct device_node *node;
	int irq, ret;

	node = of_find_compatible_node(NULL, NULL, MTK_LPM_SLEEP_COMPATIBLE_STRING);
	if (!node)
		pr_info("[name:spm&][SPM] %s: node %s not found.\n", __func__,
			MTK_LPM_SLEEP_COMPATIBLE_STRING);

	irq = irq_of_parse_and_map(node, 0);
	if (!irq) {
		pr_info("[name:spm&][SPM] failed to get spm irq\n");
		goto FINISHED;
	}

	ret = request_irq(irq, spm_irq_handler, 0, "spm-irq", NULL);
	if (ret) {
		pr_info("[name:spm&][SPM] failed to install spm irq handler, ret = %d\n", ret);
		goto FINISHED;
	}

	ret = enable_irq_wake(irq);
	if (ret) {
		pr_info("[name:spm&][SPM] failed to enable spm irq wake, ret = %d\n", ret);
		goto FINISHED;
	}

	/* tell ATF spm driver that spm irq pending number */
	spm_irq_number = virq_to_hwirq(irq);
	ret = lpm_smc_spm(MT_SPM_SMC_UID_SET_PENDING_IRQ_INIT,
		 MT_LPM_SMC_ACT_SET, spm_irq_number, 0);
	if (ret) {
		pr_info("[name:spm&][SPM] failed to nofity ATF spm irq\n", ret);
		goto FINISHED;
	}

	pr_info("[name:spm&][SPM] %s: install spm irq %d\n", __func__, spm_irq_number);
FINISHED:
	return 0;
}

#endif

static int lpm_s2idle_barrier(void)
{
	int i;
	struct cpuidle_driver *drv = cpuidle_get_driver();
	unsigned int s2idle_block_value;

	if (!drv)
		return -1;

	i = drv->state_count - 1;
	if (strcmp(drv->states[i].name, S2IDLE_STATE_NAME))
		return -1;

	/* request PM QoS between S2ilde and mcusys off */
	s2idle_block_value = (drv->states[i].exit_latency +
			      drv->states[i-1].exit_latency)/2;

	cpu_latency_qos_add_request(&lpm_qos_request, s2idle_block_value);

	return 0;
}

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

		ret = lpm_s2idle_barrier();
		if (ret)
			pr_debug("[name:spm&][SPM] Failed to set s2idle barrier.\n");
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

	ret = lpm_init_spm_irq();
	if (ret) {
		pr_debug("[name:spm&][SPM] Failed to register SPM irq.\n");
		return ret;
	}

#endif /* CONFIG_PM */

	return 0;
}
