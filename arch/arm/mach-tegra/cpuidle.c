/*
 * arch/arm/mach-tegra/cpuidle.c
 *
 * CPU idle driver for Tegra CPUs
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.
 * Copyright (c) 2011 Google, Inc.
 * Author: Colin Cross <ccross@android.com>
 *         Gary King <gking@nvidia.com>
 *
 * Rework for 3.3 by Peter De Schrijver <pdeschrijver@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/tick.h>
#include <linux/cpu_pm.h>
#include <linux/module.h>
#include <linux/hrtimer.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include <trace/events/nvpower.h>

#include "cpuidle.h"
#include "pm.h"
#include "sleep.h"
#include "timer.h"

int tegra_pg_exit_latency;
static int tegra_pd_power_off_time;
static unsigned int tegra_pd_min_residency;

extern void tegra_cpu_wfi(void);

static int tegra_idle_enter_clock_gating(struct cpuidle_device *dev,
				int index);

struct cpuidle_driver tegra_idle_driver = {
	.name = "tegra_idle",
	.owner = THIS_MODULE,
};

static int tegra_idle_enter_clock_gating(struct cpuidle_device *dev,
	int index)
{
	ktime_t enter, exit;
	s64 us;

	/* cpu_idle calls us with IRQs disabled */

	local_fiq_disable();

	enter = ktime_get();

	cpu_do_idle();

	exit = ktime_sub(ktime_get(), enter);
	us = ktime_to_us(exit);

	local_fiq_enable();

	/* cpu_idle expects us to return with IRQs enabled */
	local_irq_enable();

	dev->last_residency = us;
	return index;
}

static bool power_down_in_idle __read_mostly;

#ifdef CONFIG_PM_SLEEP
static bool pd_in_idle_modifiable __read_mostly = true;
static bool pd_disabled_by_suspend;
static struct tegra_cpuidle_ops tegra_idle_ops;

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
u32 tegra_force_clkgt_at_vmin;
#endif

void tegra_pd_in_idle(bool enable)
{
	/*
	 * If power down in idle is permanently disabled it can't be
	 * re-enabled.
	 */
	if (pd_in_idle_modifiable) {
		power_down_in_idle = enable;
		pd_in_idle_modifiable = enable;
		if (!enable)
			pr_warn("LP2 in idle disabled\n");
	}
}

void tegra_pd_update_target_residency(struct cpuidle_state *state)
{
	state->target_residency = state->exit_latency +
		tegra_pd_power_off_time;
	if (state->target_residency < tegra_pd_min_residency)
		state->target_residency = tegra_pd_min_residency;
}

static int tegra_idle_enter_pd(struct cpuidle_device *dev,
	int index)
{
	ktime_t enter, exit;
	s64 us;
	struct cpuidle_state *state = &dev->states[index];
	bool powered_down;

	if (!power_down_in_idle || pd_disabled_by_suspend ||
	    !tegra_idle_ops.pd_is_allowed(dev, state)) {
		return dev->states[dev->safe_state_index].enter(dev,
					dev->safe_state_index);
	}

	/* cpu_idle calls us with IRQs disabled */
	trace_nvcpu_powergate_rcuidle(NVPOWER_CPU_POWERGATE_ENTRY);

	enter = ktime_get();

	tegra_idle_ops.cpu_idle_stats_pd_ready(dev->cpu);
	powered_down = tegra_idle_ops.tegra_idle_pd(dev, state);

	trace_nvcpu_powergate_rcuidle(NVPOWER_CPU_POWERGATE_EXIT);

	exit = ktime_sub(ktime_get(), enter);
	us = ktime_to_us(exit);

	/* cpu_idle expects us to return with IRQs enabled */
	local_irq_enable();

	smp_rmb();

	/* Update LP2 latency provided no fall back to clock gating */
	if (powered_down) {
		tegra_pd_set_global_latency(state);
		tegra_pd_update_target_residency(state);
	}
	tegra_idle_ops.cpu_idle_stats_pd_time(dev->cpu, us);

	dev->last_residency = (int)us;
	return (powered_down) ? index : 0;
}
#endif

static int tegra_cpuidle_register_device(unsigned int cpu)
{
	struct cpuidle_device *dev;
	struct cpuidle_state *state;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->state_count = 0;
	dev->cpu = cpu;
	dev->power_specified = 1;

	state = &dev->states[0];
	snprintf(state->name, CPUIDLE_NAME_LEN, "clock-gated");
	snprintf(state->desc, CPUIDLE_DESC_LEN, "CPU clock gated");
	state->exit_latency = 10;
	state->target_residency = 10;
	state->power_usage = 600;
	state->flags = CPUIDLE_FLAG_TIME_VALID;
	state->enter = tegra_idle_enter_clock_gating;
	dev->safe_state_index = 0;
	dev->state_count++;

#ifdef CONFIG_PM_SLEEP
	state = &dev->states[1];
	snprintf(state->name, CPUIDLE_NAME_LEN, "powered-down");
	snprintf(state->desc, CPUIDLE_DESC_LEN, "CPU power gated");
	state->exit_latency = tegra_cpu_power_good_time();
	state->target_residency = tegra_cpu_power_off_time() +
		tegra_cpu_power_good_time();
	if (state->target_residency < tegra_pd_min_residency)
		state->target_residency = tegra_pd_min_residency;
	state->power_usage = 0;
	state->flags = CPUIDLE_FLAG_TIME_VALID;
	state->enter = tegra_idle_enter_pd;
	dev->state_count++;
#endif

	if (cpuidle_register_device(dev)) {
		pr_err("CPU%u: failed to register idle device\n", cpu);
		kfree(dev);
		return -EIO;
	}
	return 0;
}

static int tegra_cpuidle_pm_notify(struct notifier_block *nb,
	unsigned long event, void *dummy)
{
#ifdef CONFIG_PM_SLEEP
	if (event == PM_SUSPEND_PREPARE)
		pd_disabled_by_suspend = true;
	else if (event == PM_POST_SUSPEND)
		pd_disabled_by_suspend = false;
#endif

	return NOTIFY_OK;
}

static struct notifier_block tegra_cpuidle_pm_notifier = {
	.notifier_call = tegra_cpuidle_pm_notify,
};

static int __init tegra_cpuidle_init(void)
{
	unsigned int cpu;
	int ret;

	ret = cpuidle_register_driver(&tegra_idle_driver);
	if (ret) {
		pr_err("CPUidle driver registration failed\n");
		return ret;
	}

#ifdef CONFIG_PM_SLEEP
	tegra_pd_min_residency = tegra_cpu_lp2_min_residency();
	tegra_pg_exit_latency = tegra_cpu_power_good_time();
	tegra_pd_power_off_time = tegra_cpu_power_off_time();

	tegra_cpuidle_init_soc(&tegra_idle_ops);
#endif
	for_each_possible_cpu(cpu) {
		ret = tegra_cpuidle_register_device(cpu);
		if (ret) {
			pr_err("CPU%u: CPUidle device registration failed\n",
				cpu);
			return ret;
		}
	}

	register_pm_notifier(&tegra_cpuidle_pm_notifier);
	return 0;
}
device_initcall(tegra_cpuidle_init);

static void __exit tegra_cpuidle_exit(void)
{
	unregister_pm_notifier(&tegra_cpuidle_pm_notifier);
	cpuidle_unregister_driver(&tegra_idle_driver);
}
module_exit(tegra_cpuidle_exit);

static int pd_in_idle_set(const char *arg, const struct kernel_param *kp)
{
#ifdef CONFIG_PM_SLEEP
	int ret;

	/*
	 * If power down in idle is permanently disabled it can't be
	 * re-enabled.
	 */
	if (pd_in_idle_modifiable) {
		ret = param_set_bool(arg, kp);
		return ret;
	}
#endif
	return -ENODEV;
}

static int pd_in_idle_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops pd_in_idle_ops = {
	.set = pd_in_idle_set,
	.get = pd_in_idle_get,
};
module_param_cb(power_down_in_idle, &pd_in_idle_ops, &power_down_in_idle, 0644);

#if defined(CONFIG_DEBUG_FS) && defined(CONFIG_PM_SLEEP)
static int tegra_pd_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_idle_ops.pd_debug_show,
				inode->i_private);
}

static const struct file_operations tegra_pd_debug_ops = {
	.open		= tegra_pd_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_cpuidle_debug_init(void)
{
	struct dentry *dir;
	struct dentry *d;

	dir = debugfs_create_dir("cpuidle", NULL);
	if (!dir)
		return -ENOMEM;

	d = debugfs_create_file("power_down_stats", S_IRUGO, dir, NULL,
		&tegra_pd_debug_ops);
	if (!d)
		return -ENOMEM;

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	d = debugfs_create_x32("force_clkgt_at_vmin", S_IRUGO | S_IWUSR,
		dir, &tegra_force_clkgt_at_vmin);
	if (!d)
		return -ENOMEM;
#endif

	return 0;
}

late_initcall(tegra_cpuidle_debug_init);
#endif
