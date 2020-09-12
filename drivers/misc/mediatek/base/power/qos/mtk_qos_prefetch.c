/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/cpu_pm.h>
#include <linux/cpufreq.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/smp.h>

#include "mtk_qos_ipi.h"
#include "mtk_qos_prefetch.h"
#include <mtk_qos_sram.h>
#include <linux/arm-smccc.h>
#include <mtk_secure_api.h>

/* #define QOS_PREFETCH_USE_SMP_CALL */
/* #define QOS_PREFETCH_USE_TIMER */

#define CLUSTER0_MASK 0xFF
#define QOS_PREFETCH_VALUE(v, i) (((v) >> ((i) * 4)) & 0xf)

#define PREFETCH_DEFAULT_VAL (0xffffffff)
unsigned int prefetch_val = PREFETCH_DEFAULT_VAL;

static int qos_prefetch_cpumask = CLUSTER0_MASK;
spinlock_t qos_prefetch_cpumask_lock;

static int qos_prefetch_enabled;
static int qos_prefetch_forced;
static int qos_prefetch_log_enabled;
static unsigned int qos_prefetch_count;
static unsigned int qos_prefetch_buf[3];
static BLOCKING_NOTIFIER_HEAD(qos_prefetch_chain_head);

#ifdef QOS_PREFETCH_USE_TIMER
#define QOS_PREFETCH_TIME msecs_to_jiffies(1000)
struct timer_list qos_prefetch_timer;
#endif /* QOS_PREFETCH_USE_TIMER */

static void qos_prefetch_set_prefetch(void *info)
{
	unsigned long level = *(unsigned int *)info;
	struct arm_smccc_res res;

	// level
	// 0: default
	// 1: ctrl 1
	// 2: ctrl 2
	// 3: ctrl 3
	arm_smccc_smc(MTK_SIP_CACHE_CONTROL,
			1, level, 0, 0, 0, 0, 0, &res);
}

static void qos_prefetch_update_single(int target_cpu, int val)
{
#ifdef QOS_PREFETCH_USE_SMP_CALL
	int cpu = smp_processor_id();

	if (cpu != target_cpu)
		smp_call_function_single(cpu, qos_prefetch_set_prefetch,
				&val, 1);
	else
#endif /* QOS_PREFETCH_USE_SMP_CALL */
		qos_prefetch_set_prefetch(&val);
}

int is_qos_prefetch_enabled(void)
{
	return qos_prefetch_enabled;
}
EXPORT_SYMBOL(is_qos_prefetch_enabled);

#ifdef CONFIG_CPU_PM
static int qos_prefetch_cpuhp_online(unsigned int cpu)
{
	unsigned long spinlock_flags;
	unsigned int val;

	if (prefetch_val == PREFETCH_DEFAULT_VAL)
		return 0;

	spin_lock_irqsave(&qos_prefetch_cpumask_lock,
			spinlock_flags);

	qos_prefetch_cpumask |= BIT(cpu);

	if (is_qos_prefetch_enabled()) {
		val = QOS_PREFETCH_VALUE(prefetch_val, cpu);
		if (val)
			qos_prefetch_update_single(cpu, val);
	}

	spin_unlock_irqrestore(&qos_prefetch_cpumask_lock,
			spinlock_flags);

	return 0;
}

static int qos_prefetch_cpuhp_offline(unsigned int cpu)
{
	unsigned long spinlock_flags;

	spin_lock_irqsave(&qos_prefetch_cpumask_lock,
			spinlock_flags);

	qos_prefetch_cpumask &= ~BIT(cpu);

	spin_unlock_irqrestore(&qos_prefetch_cpumask_lock,
			spinlock_flags);

	return 0;
}

static int qos_prefetch_sched_pm_notifier(struct notifier_block *self,
		unsigned long cmd, void *v)
{
	unsigned int cpu = smp_processor_id();

	if (cmd == CPU_PM_EXIT)
		qos_prefetch_cpuhp_online(cpu);
	else if (cmd == CPU_PM_ENTER)
		qos_prefetch_cpuhp_offline(cpu);

	return NOTIFY_OK;
}

static struct notifier_block qos_prefetch_sched_pm_notifier_block = {
	.notifier_call = qos_prefetch_sched_pm_notifier,
};

static void qos_prefetch_sched_pm_init(void)
{
	cpu_pm_register_notifier(&qos_prefetch_sched_pm_notifier_block);
}
#else
static inline void qos_prefetch_sched_pm_init(void) { }
#endif /* CONFIG_CPU_PM */

void qos_prefetch_enable(int enable)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	struct qos_ipi_data qos_ipi_d;

	qos_ipi_d.cmd = QOS_IPI_QOS_PREFETCH_ENABLE;
	qos_ipi_d.u.qos_prefetch_enable.enable = enable;
	qos_ipi_to_sspm_command(&qos_ipi_d, 2);
#endif
	qos_prefetch_enabled = enable;
}
EXPORT_SYMBOL(qos_prefetch_enable);

void qos_prefetch_force(int force)
{
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	struct qos_ipi_data qos_ipi_d;

	/* index from 1 to 4 */
	if (force > 0 && force <= 4)
		qos_prefetch_forced = force;
	else
		qos_prefetch_forced = 0;

	qos_ipi_d.cmd = QOS_IPI_QOS_PREFETCH_FORCE;
	qos_ipi_d.u.qos_prefetch_force.force = qos_prefetch_forced;
	qos_ipi_to_sspm_command(&qos_ipi_d, 2);
#else
	qos_prefetch_forced = 0;
#endif
}
EXPORT_SYMBOL(qos_prefetch_force);

int is_qos_prefetch_force(void)
{
	return qos_prefetch_forced;
}
EXPORT_SYMBOL(is_qos_prefetch_force);

int is_qos_prefetch_log_enabled(void)
{
	return qos_prefetch_log_enabled;
}
EXPORT_SYMBOL(is_qos_prefetch_log_enabled);

void qos_prefetch_log_enable(int enable)
{
	qos_prefetch_log_enabled = enable;
}
EXPORT_SYMBOL(qos_prefetch_log_enable);

unsigned int get_qos_prefetch_count(void)
{
	return qos_prefetch_count;
}
EXPORT_SYMBOL(get_qos_prefetch_count);

unsigned int *get_qos_prefetch_buf(void)
{
	return qos_prefetch_buf;
}
EXPORT_SYMBOL(get_qos_prefetch_buf);

int register_prefetch_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register
		(&qos_prefetch_chain_head, nb);
}
EXPORT_SYMBOL(register_prefetch_notifier);

int unregister_prefetch_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister
		(&qos_prefetch_chain_head, nb);
}
EXPORT_SYMBOL(unregister_prefetch_notifier);

int prefetch_notifier_call_chain(unsigned long val, void *v)
{
	int ret = NOTIFY_DONE;
	int i;

	if (!is_qos_prefetch_enabled())
		return ret;

	if (val > 0 && val <= 4) {
		for (i = 0; (val & BIT(i)) == 0; i++)
			;
		qos_prefetch_count++;
		qos_prefetch_buf[i]++;
	}

	if (is_qos_prefetch_log_enabled()) {
		pr_info("#@# %s(%d) val 0x%lx\n",
				__func__, __LINE__, val);
	}

	ret = blocking_notifier_call_chain(&qos_prefetch_chain_head, val, v);

	return notifier_to_errno(ret);
}
EXPORT_SYMBOL(prefetch_notifier_call_chain);

void qos_prefetch_update_all(void)
{
#ifdef QOS_PREFETCH_STATUS_OFFSET
	unsigned long spinlock_flags;
	unsigned int val;
	int cpu_live;
	int i;

	spin_lock_irqsave(&qos_prefetch_cpumask_lock,
			spinlock_flags);

	prefetch_val = qos_sram_read(QOS_PREFETCH_STATUS_OFFSET);

	for (i = 0; i < 8; i++) {

		if (!cpu_online(i) || cpu_isolated(i))
			continue;

		cpu_live = qos_prefetch_cpumask & BIT(i);
		if (cpu_live) {
			val = QOS_PREFETCH_VALUE(prefetch_val, i);

			if (val)
				qos_prefetch_update_single(i, val);
		}
	}

	spin_unlock_irqrestore(&qos_prefetch_cpumask_lock,
			spinlock_flags);
#endif /* QOS_PREFETCH_STATUS_OFFSET */
}
EXPORT_SYMBOL(qos_prefetch_update_all);

#ifdef QOS_PREFETCH_USE_TIMER
static void qos_prefetch_timer_fn(unsigned long data)
{
	if (is_qos_prefetch_enabled())
		qos_prefetch_update_all();
}
#endif /* QOS_PREFETCH_USE_TIMER */

#ifdef CONFIG_CPU_FREQ
static int cpu_freq_old;

static int qos_prefetch_cpu_freq_callback(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	int cpu = freq->cpu;
#ifdef QOS_PREFETCH_USE_TIMER
	unsigned long expires;
#endif /* QOS_PREFETCH_USE_TIMER */

	if (cpu < 4)
		return NOTIFY_OK;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		if (freq->new == cpu_freq_old)
			return NOTIFY_OK;

		cpu_freq_old = freq->new;

		if (is_qos_prefetch_enabled()) {
			qos_prefetch_update_all();

#ifdef QOS_PREFETCH_USE_TIMER
			expires = jiffies + QOS_PREFETCH_TIME;
			mod_timer(&qos_prefetch_timer, expires);
#endif /* QOS_PREFETCH_USE_TIMER */
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block qos_prefetch_cpu_freq_notifier = {
	.notifier_call = qos_prefetch_cpu_freq_callback,
};
#endif

void qos_prefetch_tick(int cpu)
{
	unsigned int val;

	if (!is_qos_prefetch_enabled())
		return;

	if (prefetch_val == PREFETCH_DEFAULT_VAL)
		return;

	val = QOS_PREFETCH_VALUE(prefetch_val, cpu);

	if (val)
		qos_prefetch_update_single(cpu, val);
}
EXPORT_SYMBOL(qos_prefetch_tick);

void qos_prefetch_init(void)
{
	spin_lock_init(&qos_prefetch_cpumask_lock);
	qos_prefetch_sched_pm_init();

#ifdef CONFIG_CPU_FREQ
	cpufreq_register_notifier(&qos_prefetch_cpu_freq_notifier,
			CPUFREQ_TRANSITION_NOTIFIER);
#endif

#ifdef QOS_PREFETCH_USE_TIMER
	init_timer_deferrable(&qos_prefetch_timer);
	qos_prefetch_timer.function = qos_prefetch_timer_fn;
	qos_prefetch_timer.data = 0;
#endif /* QOS_PREFETCH_USE_TIMER */

	qos_prefetch_enable(0);
}
EXPORT_SYMBOL(qos_prefetch_init);

