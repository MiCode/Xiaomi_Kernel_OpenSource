/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <mach/msm_dcvs.h>

struct cpu_idle_info {
	int cpu;
	int enabled;
	int handle;
	struct msm_dcvs_idle dcvs_notifier;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct cpu_idle_info, cpu_idle_info);
static DEFINE_PER_CPU_SHARED_ALIGNED(u64, iowait_on_cpu);
static char core_name[NR_CPUS][10];
static struct pm_qos_request qos_req;
static uint32_t latency;

static int msm_dcvs_idle_notifier(struct msm_dcvs_idle *self,
		enum msm_core_control_event event)
{
	struct cpu_idle_info *info = container_of(self,
				struct cpu_idle_info, dcvs_notifier);

	switch (event) {
	case MSM_DCVS_ENABLE_IDLE_PULSE:
		info->enabled = true;
		break;

	case MSM_DCVS_DISABLE_IDLE_PULSE:
		info->enabled = false;
		break;

	case MSM_DCVS_ENABLE_HIGH_LATENCY_MODES:
		pm_qos_update_request(&qos_req, PM_QOS_DEFAULT_VALUE);
		break;

	case MSM_DCVS_DISABLE_HIGH_LATENCY_MODES:
		pm_qos_update_request(&qos_req, latency);
		break;
	}

	return 0;
}

static int msm_cpuidle_notifier(struct notifier_block *self, unsigned long cmd,
		void *v)
{
	struct cpu_idle_info *info =
		&per_cpu(cpu_idle_info, smp_processor_id());
	u64 io_wait_us = 0;
	u64 prev_io_wait_us = 0;
	u64 last_update_time = 0;
	u64 val = 0;
	uint32_t iowaited = 0;

	if (!info->enabled)
		return NOTIFY_OK;

	switch (cmd) {
	case CPU_PM_ENTER:
		val = get_cpu_iowait_time_us(smp_processor_id(),
					&last_update_time);
		/* val could be -1 when NOHZ is not enabled */
		if (val == (u64)-1)
			val = 0;
		per_cpu(iowait_on_cpu, smp_processor_id()) = val;
		msm_dcvs_idle(info->handle, MSM_DCVS_IDLE_ENTER, 0);
		break;

	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		prev_io_wait_us = per_cpu(iowait_on_cpu, smp_processor_id());
		val = get_cpu_iowait_time_us(smp_processor_id(),
				&last_update_time);
		if (val == (u64)-1)
			val = 0;
		io_wait_us = val;
		iowaited = (io_wait_us - prev_io_wait_us);
		msm_dcvs_idle(info->handle, MSM_DCVS_IDLE_EXIT, iowaited);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block idle_nb = {
	.notifier_call = msm_cpuidle_notifier,
};

static int msm_dcvs_idle_probe(struct platform_device *pdev)
{
	int cpu;
	struct cpu_idle_info *info = NULL;
	struct msm_dcvs_idle *inotify = NULL;

	for_each_possible_cpu(cpu) {
		info = &per_cpu(cpu_idle_info, cpu);
		info->cpu = cpu;
		inotify = &info->dcvs_notifier;
		snprintf(core_name[cpu], 10, "cpu%d", cpu);
		inotify->core_name = core_name[cpu];
		inotify->enable = msm_dcvs_idle_notifier;
		info->handle = msm_dcvs_idle_source_register(inotify);
		BUG_ON(info->handle < 0);
	}

	latency = *((uint32_t *)pdev->dev.platform_data);
	pm_qos_add_request(&qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	return cpu_pm_register_notifier(&idle_nb);
}

static int msm_dcvs_idle_remove(struct platform_device *pdev)
{
	int ret = 0;
	int rc = 0;
	int cpu = 0;
	struct msm_dcvs_idle *inotify = NULL;
	struct cpu_idle_info *info = NULL;

	rc = cpu_pm_unregister_notifier(&idle_nb);

	for_each_possible_cpu(cpu) {
		info = &per_cpu(cpu_idle_info, cpu);
		inotify = &info->dcvs_notifier;
		ret = msm_dcvs_idle_source_unregister(inotify);
		if (ret) {
			rc = -EFAULT;
			pr_err("Error de-registering core %d idle notifier.\n",
					cpu);
		}
	}

	return rc;
}

static struct platform_driver idle_pdrv = {
	.probe = msm_dcvs_idle_probe,
	.remove = __devexit_p(msm_dcvs_idle_remove),
	.driver = {
		.name  = "msm_cpu_idle",
		.owner = THIS_MODULE,
	},
};

static int msm_dcvs_idle_init(void)
{
	return platform_driver_register(&idle_pdrv);
}
late_initcall(msm_dcvs_idle_init);
