/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/workqueue.h>

static unsigned int swpm_arm_pmu_status;

static void swpm_perf_arm_pmu_init(struct work_struct *work);
static struct workqueue_struct *swpm_perf_arm_pmu_init_work_queue;
DECLARE_WORK(swpm_perf_arm_pmu_init_work, swpm_perf_arm_pmu_init);

static DEFINE_MUTEX(swpm_pmu_lock);

static DEFINE_PER_CPU(struct perf_event *, l3dc_events);
static DEFINE_PER_CPU(struct perf_event *, inst_spec_events);
static DEFINE_PER_CPU(struct perf_event *, cycle_events);

static struct perf_event_attr l3dc_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config		= ARMV8_PMUV3_PERFCTR_L3D_CACHE, /* 0x2B */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/* 	.disabled       = 1, */
//	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};
static struct perf_event_attr inst_spec_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x1B, */
	.config		= ARMV8_PMUV3_PERFCTR_INST_SPEC, /*0 x1B */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
//	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};
static struct perf_event_attr cycle_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_CPU_CYCLES,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/* 	.disabled       = 1, */
//	.sample_period  = 0, /* 1000000000, */ /* ns ? */
};

static int swpm_arm_pmu_enable(int cpu, int enable)
{
	struct perf_event *event;
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);

	if (enable) {
		if (!l3_event) {
			event = perf_event_create_kernel_counter(
				&l3dc_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l3dc counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l3dc_events, cpu) = event;
		}
		if (!i_event) {
			event = perf_event_create_kernel_counter(
				&inst_spec_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) inst_spec counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(inst_spec_events, cpu) = event;
		}
		if (!c_event) {
			event = perf_event_create_kernel_counter(
				&cycle_event_attr, cpu,	NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) cycle counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(cycle_events, cpu) = event;
		}
		if (l3_event)
			perf_event_enable(l3_event);
		if (i_event)
			perf_event_enable(i_event);
		if (c_event)
			perf_event_enable(c_event);
	} else {
		if (l3_event)
			perf_event_disable(l3_event);
		if (i_event)
			perf_event_disable(i_event);
		if (c_event)
			perf_event_disable(c_event);

		if (l3_event) {
			per_cpu(l3dc_events, cpu) = NULL;
			perf_event_release_kernel(l3_event);
		}
		if (i_event) {
			per_cpu(inst_spec_events, cpu) = NULL;
			perf_event_release_kernel(i_event);
		}
		if (c_event) {
			per_cpu(cycle_events, cpu) = NULL;
			perf_event_release_kernel(c_event);
		}
	}

	return 0;
FAIL:
        return (int)PTR_ERR(event);
}

int swpm_arm_pmu_get_status(void)
{
	return swpm_arm_pmu_status;
}
EXPORT_SYMBOL(swpm_arm_pmu_get_status);

int swpm_arm_pmu_enable_all(unsigned int enable)
{
	int i, ret = 0;

	mutex_lock(&swpm_pmu_lock);

	get_online_cpus();
	for (i = 0; i < num_possible_cpus(); i++)
		ret |= swpm_arm_pmu_enable(i, !!enable);
	put_online_cpus();

	if (!ret)
		swpm_arm_pmu_status = !!enable;

	mutex_unlock(&swpm_pmu_lock);

	return ret;
}
EXPORT_SYMBOL(swpm_arm_pmu_enable_all);

static void swpm_perf_arm_pmu_init(struct work_struct *work)
{
	swpm_arm_pmu_enable_all(1);
}

int __init swpm_arm_pmu_init(void)
{
	if (!swpm_perf_arm_pmu_init_work_queue) {
		swpm_perf_arm_pmu_init_work_queue =
			create_workqueue("swpm_perf_arm_pmu_init");
		if (!swpm_perf_arm_pmu_init_work_queue)
			pr_debug("swpm_perf_arm_pmu_init workqueue create failed\n");
	}

	if (swpm_perf_arm_pmu_init_work_queue)
		queue_work(swpm_perf_arm_pmu_init_work_queue,
			   &swpm_perf_arm_pmu_init_work);

	return 0;
}
module_init(swpm_arm_pmu_init);

void __exit swpm_arm_pmu_exit(void)
{
}
module_exit(swpm_arm_pmu_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek SWPM ARM PMU Module");
MODULE_AUTHOR("MediaTek Inc.");
