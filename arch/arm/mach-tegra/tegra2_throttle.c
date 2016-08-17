/*
 * arch/arm/mach-tegra/tegra2_throttle.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Based on arch/arm/plat-omap/cpu-omap.c, (C) 2005 Nokia Corporation
 *
 * Copyright (C) 2010-2013 NVIDIA Corporation
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>

#include "clock.h"
#include "cpu-tegra.h"

/* tegra throttling require frequencies in the table to be in ascending order */
static struct cpufreq_frequency_table *throttle_table;

/* cpu_throttle_lock is tegra_cpu_lock from cpu-tegra.c */
static struct mutex *cpu_throttle_lock;

/* CPU frequency is gradually lowered when throttling is enabled */
#define THROTTLE_DELAY		msecs_to_jiffies(2000)

static int is_throttling;
static int throttle_lowest_index;
static int throttle_highest_index;
static int throttle_index;
static int throttle_next_index;
static struct delayed_work throttle_work;
static struct workqueue_struct *workqueue;
static DEFINE_MUTEX(tegra_throttle_lock);

static void tegra_throttle_work_func(struct work_struct *work)
{
	unsigned int current_freq;

	mutex_lock(cpu_throttle_lock);
	if (!is_throttling)
		goto out;

	current_freq = tegra_getspeed(0);
	throttle_index = throttle_next_index;

	if (throttle_table[throttle_index].frequency < current_freq)
		tegra_cpu_set_speed_cap_locked(NULL);

	if (throttle_index > throttle_lowest_index) {
		throttle_next_index = throttle_index - 1;
		queue_delayed_work(workqueue, &throttle_work, THROTTLE_DELAY);
	}
out:
	mutex_unlock(cpu_throttle_lock);
}

/*
 * tegra_throttling_enable
 * This function may sleep
 */
void tegra_throttling_enable(bool enable)
{
	mutex_lock(&tegra_throttle_lock);
	mutex_lock(cpu_throttle_lock);

	if (enable && !(is_throttling++)) {
		unsigned int current_freq = tegra_getspeed(0);

		for (throttle_index = throttle_highest_index;
		     throttle_index >= throttle_lowest_index;
		     throttle_index--)
			if (throttle_table[throttle_index].frequency
			    < current_freq)
				break;

		throttle_index = max(throttle_index, throttle_lowest_index);
		throttle_next_index = throttle_index;
		queue_delayed_work(workqueue, &throttle_work, 0);
	} else if (!enable && is_throttling) {
		if (!(--is_throttling)) {
			/* restore speed requested by governor */
			tegra_cpu_set_speed_cap_locked(NULL);

			mutex_unlock(cpu_throttle_lock);
			cancel_delayed_work_sync(&throttle_work);
			mutex_unlock(&tegra_throttle_lock);
			return;
		}
	}
	mutex_unlock(cpu_throttle_lock);
	mutex_unlock(&tegra_throttle_lock);
}
EXPORT_SYMBOL_GPL(tegra_throttling_enable);

unsigned int tegra_throttle_governor_speed(unsigned int requested_speed)
{
	return is_throttling ?
		min(requested_speed, throttle_table[throttle_index].frequency) :
		requested_speed;
}

bool tegra_is_throttling(int *count)
{
	return is_throttling;
}

int __init tegra_throttle_init(struct mutex *cpu_lock)
{
	struct tegra_cpufreq_table_data *table_data =
		tegra_cpufreq_table_get();
	if (IS_ERR_OR_NULL(table_data))
		return -EINVAL;

	/*
	 * High-priority, others flags default: not bound to a specific
	 * CPU, has rescue worker task (in case of allocation deadlock,
	 * etc.).  Single-threaded.
	 */
	workqueue = alloc_workqueue("cpu-tegra",
				    WQ_HIGHPRI | WQ_UNBOUND | WQ_RESCUER, 1);
	if (!workqueue)
		return -ENOMEM;
	INIT_DELAYED_WORK(&throttle_work, tegra_throttle_work_func);

	throttle_lowest_index = table_data->throttle_lowest_index;
	throttle_highest_index = table_data->throttle_highest_index;
	throttle_table = table_data->freq_table;
	cpu_throttle_lock = cpu_lock;

	return 0;
}

void tegra_throttle_exit(void)
{
	destroy_workqueue(workqueue);
}

#ifdef CONFIG_DEBUG_FS

static int throttle_debug_set(void *data, u64 val)
{
	tegra_throttling_enable(val);
	return 0;
}
static int throttle_debug_get(void *data, u64 *val)
{
	*val = (u64) is_throttling;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(throttle_fops, throttle_debug_get, throttle_debug_set,
			"%llu\n");

int __init tegra_throttle_debug_init(struct dentry *cpu_tegra_debugfs_root)
{
	if (!debugfs_create_file("throttle", 0644, cpu_tegra_debugfs_root,
				 NULL, &throttle_fops))
		return -ENOMEM;
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

