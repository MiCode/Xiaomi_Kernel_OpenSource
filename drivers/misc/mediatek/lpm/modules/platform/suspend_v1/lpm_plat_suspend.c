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
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
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

long long before_md_sleep_time;
long long after_md_sleep_time;

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
#define MD_SLEEP_INFO_SMEM_OFFEST (4)
u32 *share_mem;
struct md_sleep_status before_md_sleep_status;
struct md_sleep_status after_md_sleep_status;
#endif

static void get_md_sleep_time_addr(void)
{
	/* dump subsystem sleep info */
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	int ret;
	u64 of_find;
	struct device_node *mddriver = NULL;

	mddriver = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (!mddriver) {
		pr_info("mddriver not found in DTS\n");
		return;
	}

	ret =  of_property_read_u64(mddriver, "md_low_power_addr", &of_find);

	if (ret) {
		pr_info("address not found in DTS");
		return;
	}

	share_mem = (u32 *)ioremap_wc(of_find, 0x200);

	if (share_mem == NULL) {
		pr_info("[name:spm&][%s:%d] - No MD share mem\n",
			 __func__, __LINE__);
		return;
	}

	share_mem = share_mem + MD_SLEEP_INFO_SMEM_OFFEST;
#endif
}

static void get_md_sleep_time(struct md_sleep_status *md_data)
{
	if (!md_data)
		return;

	/* dump subsystem sleep info */
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	if (share_mem ==  NULL) {
		pr_info("MD shared memory is NULL");
	} else {
		memset(md_data, 0, sizeof(struct md_sleep_status));
		memcpy(md_data, share_mem, sizeof(struct md_sleep_status));
		return;
	}
#endif
}

static void log_md_sleep_info(void)
{

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
#define LOG_BUF_SIZE	256
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;

	if (after_md_sleep_status.sleep_time >= before_md_sleep_status.sleep_time) {
		pr_info("[name:spm&][SPM] md_slp_duration = %llu (32k)\n",
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
		pr_info("[name:spm&][SPM] %s", log_buf);
	}
#endif
}

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
		issuer->log(LPM_ISSUER_SUSPEND, "suspend", (void *)issuer);
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
#if IS_ENABLED(CONFIG_PM_SLEEP)
		/* TODO
		 * Need to fix the rcu_idle workaround later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to use rcu_idle_exit to
		 * wake up RCU, or using RCU_NONIDLE() with syscore.
		 * rcu_idle_exit was called by the caller, lpm_cpuidle_prepare()
		 * But anyway in s2idle, when lastest cpu
		 * enter idle state means there won't care r/w sync problem
		 * and RCU_NONIDLE() maybe the right solution.
		 */
		syscore_suspend();
#endif
		pr_info("[name:spm&][%s:%d] - suspend enter\n",
			__func__, __LINE__);

		/* Record md sleep status*/
		get_md_sleep_time(&before_md_sleep_status);

		ret = __lpm_suspend_prompt(LPM_SUSPEND_S2IDLE,
					      cpu, issuer);
	}
	return ret;
}

void lpm_suspend_s2idle_reflect(int cpu,
					const struct lpm_issuer *issuer)
{
	if (cpumask_weight(&s2idle_cpumask) == num_online_cpus()) {

		__lpm_suspend_reflect(LPM_SUSPEND_S2IDLE,
					 cpu, issuer);
	pr_info("[name:spm&][%s:%d] - resume\n",
			__func__, __LINE__);

	if ((after_md_sleep_time >= 0) && (after_md_sleep_time >= before_md_sleep_time))
		pr_info("[name:spm&][SPM] md_slp_duration = %lld",
			after_md_sleep_time - before_md_sleep_time);
	else
		pr_info("[name:spm&][SPM] md share memory is NULL");

	/* show md sleep status */
	get_md_sleep_time(&after_md_sleep_status);
	log_md_sleep_info();
#if IS_ENABLED(CONFIG_PM_SLEEP)
		/* TODO
		 * Need to fix the rcu_idle/timekeeping later.
		 * There are many rcu behaviors in syscore callback.
		 * In s2idle framework, the rcu enter idle before cpu
		 * enter idle state. So we need to use rcu_idle_exit to
		 * wake up RCU, or using RCU_NONIDLE() with syscore.
		 * rcu_idle_exit was called by the caller, lpm_cpuidle_prepare()
		 * But anyway in s2idle, when lastest cpu
		 * enter idle state means there won't care r/w sync problem
		 * and RCU_NONIDLE() maybe the right solution.
		 */
		syscore_resume();
#endif
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

#define MTK_LPM_SLEEP_COMPATIBLE_STRING "mediatek,sleep"
static int spm_irq_number = -1;
static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	pm_system_wakeup();
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
/* Do not re-register spm irq handler again when TWAM presents */
#if !IS_ENABLED(CONFIG_MTK_SPMTWAM)
	ret = request_irq(irq, spm_irq0_handler,
		IRQF_NO_SUSPEND, "spm-irq", NULL);
	if (ret) {
		pr_info("[name:spm&][SPM] failed to install spm irq handler, ret = %d\n", ret);
		goto FINISHED;
	}
#endif
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

	get_md_sleep_time_addr();

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
