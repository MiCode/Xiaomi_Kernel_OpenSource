/*
 * Copyright (C) 2011 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross <at> android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>

static DEFINE_RWLOCK(cpu_pm_notifier_lock);
static RAW_NOTIFIER_HEAD(cpu_pm_notifier_chain);

int cpu_pm_register_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&cpu_pm_notifier_lock, flags);
	ret = raw_notifier_chain_register(&cpu_pm_notifier_chain, nb);
	write_unlock_irqrestore(&cpu_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_pm_register_notifier);

int cpu_pm_unregister_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&cpu_pm_notifier_lock, flags);
	ret = raw_notifier_chain_unregister(&cpu_pm_notifier_chain, nb);
	write_unlock_irqrestore(&cpu_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_pm_unregister_notifier);

static int cpu_pm_notify(enum cpu_pm_event event, int nr_to_call, int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&cpu_pm_notifier_chain, event, NULL,
		nr_to_call, nr_calls);

	return notifier_to_errno(ret);
}

int cpu_pm_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&cpu_pm_notifier_lock);
	ret = cpu_pm_notify(CPU_PM_ENTER, -1, &nr_calls);
	if (ret)
		cpu_pm_notify(CPU_PM_ENTER_FAILED, nr_calls - 1, NULL);
	read_unlock(&cpu_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_pm_enter);

int cpu_pm_exit(void)
{
	int ret;

	read_lock(&cpu_pm_notifier_lock);
	ret = cpu_pm_notify(CPU_PM_EXIT, -1, NULL);
	read_unlock(&cpu_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_pm_exit);

int cpu_cluster_pm_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&cpu_pm_notifier_lock);
	ret = cpu_pm_notify(CPU_CLUSTER_PM_ENTER, -1, &nr_calls);
	if (ret)
		cpu_pm_notify(CPU_CLUSTER_PM_ENTER_FAILED, nr_calls - 1, NULL);
	read_unlock(&cpu_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_cluster_pm_enter);

int cpu_cluster_pm_exit(void)
{
	int ret;

	read_lock(&cpu_pm_notifier_lock);
	ret = cpu_pm_notify(CPU_CLUSTER_PM_EXIT, -1, NULL);
	read_unlock(&cpu_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(cpu_cluster_pm_exit);
