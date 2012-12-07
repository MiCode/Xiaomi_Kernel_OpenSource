/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/compiler.h>
#include <linux/notifier.h>
#include <linux/percpu.h>
#include <linux/workqueue.h>

#define MSM_CPU_SECONDARY_CORE_OFFSET	0x10000

static const phys_addr_t primary_cpu_pwrctl_phys = 0x2088004;
static DEFINE_PER_CPU(int, pll_clamp_set);
static void msm_cpu_pwrctl_work_cb(struct work_struct *work);
static __cpuinitdata DECLARE_WORK(msm_cpu_pwrctl_work, msm_cpu_pwrctl_work_cb);
static int nr_cpus_done;
static int __cpuinit msm_cpu_pwrctl_cpu_callback(struct notifier_block *nfb,
				    unsigned long action, void *hcpu);
static struct notifier_block __cpuinitdata msm_cpu_pwrctl_cpu_notifier = {
	.notifier_call = msm_cpu_pwrctl_cpu_callback,
};

static void __cpuinit msm_cpu_pwrctl_work_cb(struct work_struct *work)
{
	unregister_hotcpu_notifier(&msm_cpu_pwrctl_cpu_notifier);
}

static int __cpuinit msm_cpu_pwrctl_cpu_callback(struct notifier_block *nfb,
				    unsigned long action, void *hcpu)
{
	int cpu = (int) hcpu;
	int *pll_clamp;
	void *pwrctl_ptr;
	unsigned int value;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_ONLINE:
		pll_clamp = &per_cpu(pll_clamp_set, cpu);
		if (likely(*pll_clamp))
			goto done;

		pwrctl_ptr = ioremap_nocache(primary_cpu_pwrctl_phys +
			(cpu * MSM_CPU_SECONDARY_CORE_OFFSET), SZ_4K);
		if (unlikely(!pwrctl_ptr))
			goto done;

		value = readl_relaxed(pwrctl_ptr);
		value |= 0x100;
		writel_relaxed(value, pwrctl_ptr);
		*pll_clamp = 1;
		iounmap(pwrctl_ptr);

		if (++nr_cpus_done == cpumask_weight(cpu_possible_mask))
			schedule_work(&msm_cpu_pwrctl_work);
done:
		break;
	default:
		break;
	}

	return NOTIFY_OK;

}

static int __init msm_cpu_pwrctl_init(void)
{
	int cpu = smp_processor_id();

	/* We won't get cpu online notification for this CPU,
	 * so take this opportunity to process this CPU.
	 */
	msm_cpu_pwrctl_cpu_callback(&msm_cpu_pwrctl_cpu_notifier,
					CPU_ONLINE, (void *) cpu);

	register_hotcpu_notifier(&msm_cpu_pwrctl_cpu_notifier);
	return 0;
}

early_initcall(msm_cpu_pwrctl_init);
