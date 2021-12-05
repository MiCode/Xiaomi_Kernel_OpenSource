// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/energy_model.h>
#include <linux/topology.h>
#include <trace/hooks/topology.h>
#include <trace/events/sched.h>
#include <trace/hooks/sched.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <sched/sched.h>
#include "sched_sys_common.h"

DEFINE_MUTEX(sched_core_pause_mutex);

/*
 * Pause a cpu
 * Success or no need to change: return 0
 */
int sched_pause_cpu(int cpu)
{
	int err = 0;
	struct cpumask cpu_pause_mask;
	struct cpumask cpu_inactive_mask;

	mutex_lock(&sched_core_pause_mutex);
	cpumask_complement(&cpu_inactive_mask, cpu_active_mask);
	if (cpumask_test_cpu(cpu, &cpu_inactive_mask)) {
		pr_info("[Core Pause]Already Pause: cpu=%d, active=0x%lx, online=0x%lx\n",
			cpu, cpu_active_mask->bits[0],
			cpu_online_mask->bits[0]);
		mutex_unlock(&sched_core_pause_mutex);
		return err;
	}

	cpumask_clear(&cpu_pause_mask);
	cpumask_set_cpu(cpu, &cpu_pause_mask);

	err = pause_cpus(&cpu_pause_mask);
	if (err) {
		pr_info_ratelimited("[Core Pause]Pause fail: cpu=%d, pause=0x%lx, active=0x%lx, online=0x%lx, err=%d\n",
			cpu, cpu_pause_mask.bits[0], cpu_active_mask->bits[0],
			cpu_online_mask->bits[0], err);
	} else {
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
		set_cpu_active_bitmask(cpumask_bits(cpu_active_mask)[0]);
#endif
		pr_info("[Core Pause]Pause success: cpu=%d, pause=0x%lx, active=0x%lx, online=0x%lx\n",
			cpu, cpu_pause_mask.bits[0], cpu_active_mask->bits[0],
			cpu_online_mask->bits[0]);
	}
	mutex_unlock(&sched_core_pause_mutex);

	return err;
}
EXPORT_SYMBOL_GPL(sched_pause_cpu);

/*
 * Resume a cpu
 * Success or no need to change: return 0
 */
int sched_resume_cpu(int cpu)
{
	int err = 0;
	struct cpumask cpu_resume_mask;

	mutex_lock(&sched_core_pause_mutex);
	if (cpumask_test_cpu(cpu, cpu_active_mask)) {
		pr_info("[Core Pause]Already Resume: cpu=%d, active=0x%lx, online=0x%lx\n",
				cpu, cpu_active_mask->bits[0], cpu_online_mask->bits[0]);
		mutex_unlock(&sched_core_pause_mutex);
		return err;
	}

	cpumask_clear(&cpu_resume_mask);
	cpumask_set_cpu(cpu, &cpu_resume_mask);
	err = resume_cpus(&cpu_resume_mask);
	if (err) {
		pr_info_ratelimited("[Core Pause]Resume fail: cpu=%d, resume=0x%lx, active=0x%lx, online=0x%lx, err=%d\n",
				cpu, cpu_resume_mask.bits[0], cpu_active_mask->bits[0],
				cpu_online_mask->bits[0], err);
	} else {
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
		set_cpu_active_bitmask(cpumask_bits(cpu_active_mask)[0]);
#endif
		pr_info("[Core Pause]Resume success: cpu=%d, resume=0x%lx, active=0x%lx, online=0x%lx\n",
				cpu, cpu_resume_mask.bits[0], cpu_active_mask->bits[0],
				cpu_online_mask->bits[0]);
	}
	mutex_unlock(&sched_core_pause_mutex);

	return err;
}
EXPORT_SYMBOL_GPL(sched_resume_cpu);

static ssize_t show_sched_core_pause_info(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf+len, max_len-len,
			"cpu_active_mask=0x%lx\n",
			__cpu_active_mask.bits[0]);

	return len;
}

struct kobj_attribute sched_core_pause_info_attr =
__ATTR(sched_core_pause_info, 0400, show_sched_core_pause_info, NULL);
