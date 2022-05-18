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
#include <linux/hrtimer.h>
#include <linux/ktime.h>


#include <lpm.h>
#include <lpm_module.h>
#include <lpm_call.h>
#include <lpm_type.h>
#include <lpm_call_type.h>
#include <lpm_dbg_common_v1.h>
#include <mtk_cpuidle_status.h>

#include "lpm_plat.h"
#include "lpm_plat_comm.h"
#include "lpm_plat_suspend.h"

unsigned int lpm_suspend_status;
struct cpumask s2idle_cpumask;
static struct cpumask abort_cpumask;
static DEFINE_SPINLOCK(lpm_abort_locker);
static struct pm_qos_request lpm_qos_request;

#define S2IDLE_STATE_NAME "s2idle"

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
u32 *share_mem;
struct md_sleep_status before_md_sleep_status;
struct md_sleep_status after_md_sleep_status;
#define MD_GUARD_NUMBER 0x536C702E
#define GET_RECORD_CNT1(n) ((n >> 32) & 0xFFFFFFFF)
#define GET_RECORD_CNT2(n) (n & 0xFFFFFFFF)
#define GET_GUARD_L(n) (n & 0xFFFFFFFF)
#define GET_GUARD_H(n) ((n >> 32) & 0xFFFFFFFF)
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
#endif
}

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
static void get_md_sleep_time(struct md_sleep_status *md_data)
{
	if (!md_data)
		return;

	/* dump subsystem sleep info */
	if (share_mem ==  NULL) {
		pr_info("MD shared memory is NULL");
	} else {
		memset(md_data, 0, sizeof(struct md_sleep_status));
		memcpy(md_data, share_mem, sizeof(struct md_sleep_status));
		return;
	}
}
#endif
#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
static int is_md_sleep_info_valid(struct md_sleep_status *md_data)
{
	u32 guard_l = GET_GUARD_L(md_data->guard_sleep_cnt1);
	u32 guard_h = GET_GUARD_H(md_data->guard_sleep_cnt2);
	u32 cnt1 = GET_RECORD_CNT1(md_data->guard_sleep_cnt1);
	u32 cnt2 = GET_RECORD_CNT2(md_data->guard_sleep_cnt2);

	if ((guard_l != MD_GUARD_NUMBER) || (guard_h != MD_GUARD_NUMBER))
		return 0;

	if (cnt1 != cnt2)
		return 0;

	return 1;
}

static void log_md_sleep_info(void)
{

#define LOG_BUF_SIZE	256
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;

	if (!is_md_sleep_info_valid(&before_md_sleep_status)
		|| !is_md_sleep_info_valid(&after_md_sleep_status)) {
		pr_info("[name:spm&][SPM] MD sleep info. is not valid");
		return;
	}

	if (GET_RECORD_CNT1(after_md_sleep_status.guard_sleep_cnt1)
		== GET_RECORD_CNT1(before_md_sleep_status.guard_sleep_cnt1)) {
		pr_info("[name:spm&][SPM] MD sleep info. is not updated");
		return;
	}

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
}
#endif

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

		__lpm_suspend_reflect(LPM_SUSPEND_S2IDLE,
					 cpu, issuer);
	pr_info("[name:spm&][%s:%d] - resume\n",
			__func__, __LINE__);

#if IS_ENABLED(CONFIG_MTK_ECCCI_DRIVER)
	/* show md sleep status */
	get_md_sleep_time(&after_md_sleep_status);
	log_md_sleep_info();
#endif

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
#define CPU_NUMBER (NR_CPUS)
struct mtk_lpm_abort_control {
	struct task_struct *ts;
	int cpu;
};
static struct mtk_lpm_abort_control mtk_lpm_ac[CPU_NUMBER];
static int mtk_lpm_in_suspend;
static struct hrtimer lpm_hrtimer[NR_CPUS];
static enum hrtimer_restart lpm_hrtimer_timeout(struct hrtimer *timer)
{
	if (mtk_lpm_in_suspend) {
		pr_info("[name:spm&][SPM] wakeup system due to not entering suspend\n");
		pm_system_wakeup();
	}
	return HRTIMER_NORESTART;
}
static int mtk_lpm_monitor_thread(void *data)
{
	struct sched_param param = {.sched_priority = 99 };
	struct mtk_lpm_abort_control *lpm_ac;
	ktime_t kt;

	lpm_ac = (struct mtk_lpm_abort_control *)data;
	kt = ktime_set(5, 100000);

	sched_setscheduler(current, SCHED_FIFO, &param);
	allow_signal(SIGKILL);

	if (mtk_lpm_in_suspend) {
		hrtimer_start(&lpm_hrtimer[lpm_ac->cpu], kt, HRTIMER_MODE_REL);
		msleep_interruptible(5000);
		hrtimer_cancel(&lpm_hrtimer[lpm_ac->cpu]);
	} else {
		msleep_interruptible(5000);
	}

	spin_lock(&lpm_abort_locker);
	if (cpumask_test_cpu(lpm_ac->cpu, &abort_cpumask))
		cpumask_clear_cpu(lpm_ac->cpu, &abort_cpumask);
	spin_unlock(&lpm_abort_locker);
	do_exit(0);
}

static int suspend_online_cpus;
static int lpm_spm_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct timespec64 ts;
	struct rtc_time tm;
	int ret;
	int cpu;

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
		ret = mtk_s2idle_state_enable(1);
		if (ret)
			return NOTIFY_BAD;
		cpu_hotplug_disable();
		suspend_online_cpus = num_online_cpus();
		cpumask_clear(&abort_cpumask);
		mtk_lpm_in_suspend = 1;
		get_online_cpus();
		for_each_online_cpu(cpu) {
			mtk_lpm_ac[cpu].ts = kthread_create(mtk_lpm_monitor_thread,
					&mtk_lpm_ac[cpu], "LPM-%d", cpu);
			mtk_lpm_ac[cpu].cpu = cpu;
			if (!IS_ERR(mtk_lpm_ac[cpu].ts)) {
				cpumask_set_cpu(cpu, &abort_cpumask);
				kthread_bind(mtk_lpm_ac[cpu].ts, cpu);
				wake_up_process(mtk_lpm_ac[cpu].ts);
			} else {
				pr_info("[name:spm&][SPM] create LPM monitor thread %d fail\n",
											cpu);
				mtk_lpm_in_suspend = 0;
				/* terminate previously created threads */
				spin_lock(&lpm_abort_locker);
				if (!cpumask_empty(&abort_cpumask)) {
					for_each_cpu(cpu, &abort_cpumask) {
						send_sig(SIGKILL, mtk_lpm_ac[cpu].ts, 0);
					}
				}
				spin_unlock(&lpm_abort_locker);
				put_online_cpus();
				return NOTIFY_BAD;
			}

		}
		put_online_cpus();
		return NOTIFY_DONE;
	case PM_POST_SUSPEND:
		cpu_hotplug_enable();
		/* make sure the rest of callback proceeds*/
		mtk_s2idle_state_enable(0);
		mtk_lpm_in_suspend = 0;
		spin_lock(&lpm_abort_locker);
		if (!cpumask_empty(&abort_cpumask)) {
			pr_info("[name:spm&][SPM] check cpumask %*pb\n",
					cpumask_pr_args(&abort_cpumask));
			for_each_cpu(cpu, &abort_cpumask)
				send_sig(SIGKILL, mtk_lpm_ac[cpu].ts, 0);
		}
		spin_unlock(&lpm_abort_locker);
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

	mtk_s2idle_state_enable(0);

	return 0;
}

int __init lpm_model_suspend_init(void)
{
	int ret;
	int i;

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

	for (i = 0; i < CPU_NUMBER; i++) {
		hrtimer_init(&lpm_hrtimer[i], CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		lpm_hrtimer[i].function = lpm_hrtimer_timeout;
	}
#endif /* CONFIG_PM */

	return 0;
}
