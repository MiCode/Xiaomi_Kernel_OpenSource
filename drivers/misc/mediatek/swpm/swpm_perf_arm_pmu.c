// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/perf_event.h>
#include <linux/workqueue.h>

#include <swpm_perf_arm_pmu.h>

static unsigned int swpm_arm_pmu_status;
static unsigned int boundary;
static unsigned int rf_boundary;
static unsigned int pmu_rf_support;

static DEFINE_MUTEX(swpm_pmu_lock);

static DEFINE_PER_CPU(struct perf_event *, l3dc_events);
static DEFINE_PER_CPU(struct perf_event *, inst_spec_events);
static DEFINE_PER_CPU(struct perf_event *, cycle_events);
static DEFINE_PER_CPU(struct perf_event *, l3dc_rf_events);
static DEFINE_PER_CPU(int, l3dc_idx);
static DEFINE_PER_CPU(int, inst_spec_idx);
static DEFINE_PER_CPU(int, cycle_idx);
static DEFINE_PER_CPU(int, l3dc_rf_idx);

static struct perf_event_attr l3dc_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2B, */
	.config		= ARMV8_PMUV3_PERFCTR_L3D_CACHE, /* 0x2B */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr inst_spec_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x1B, */
	.config		= ARMV8_PMUV3_PERFCTR_INST_SPEC, /* 0x1B */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr cycle_event_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_CPU_CYCLES,
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};
static struct perf_event_attr l3dc_rf_event_attr = {
	.type           = PERF_TYPE_RAW,
/*	.config         = 0x2A, */
	.config		= ARMV8_PMUV3_PERFCTR_L3D_CACHE_REFILL, /* 0x2A */
	.size           = sizeof(struct perf_event_attr),
	.pinned         = 1,
/*	.disabled       = 1, */
};

static void swpm_pmu_start(int cpu)
{
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);
	struct perf_event *l3_rf_event = per_cpu(l3dc_rf_events, cpu);

	if (l3_event) {
		perf_event_enable(l3_event);
		per_cpu(l3dc_idx, cpu) = l3_event->hw.idx;
	}
	if (i_event) {
		perf_event_enable(i_event);
		per_cpu(inst_spec_idx, cpu) = i_event->hw.idx;
	}
	if (c_event) {
		perf_event_enable(c_event);
		per_cpu(cycle_idx, cpu) = c_event->hw.idx;
	}
	if (l3_rf_event) {
		perf_event_enable(l3_rf_event);
		per_cpu(l3dc_rf_idx, cpu) = l3_rf_event->hw.idx;
	}
}

static void swpm_pmu_stop(int cpu)
{
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *l3_rf_event = per_cpu(l3dc_rf_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);

	if (l3_event) {
		perf_event_disable(l3_event);
		per_cpu(l3dc_idx, cpu) = -1;
	}
	if (i_event) {
		perf_event_disable(i_event);
		per_cpu(inst_spec_idx, cpu) = -1;
	}
	if (c_event) {
		perf_event_disable(c_event);
		per_cpu(cycle_idx, cpu) = -1;
	}
	if (l3_rf_event) {
		perf_event_disable(l3_rf_event);
		per_cpu(l3dc_rf_idx, cpu) = -1;
	}
}

static int swpm_arm_pmu_enable(int cpu, int enable)
{
	struct perf_event *event;
	struct perf_event *l3_event = per_cpu(l3dc_events, cpu);
	struct perf_event *i_event = per_cpu(inst_spec_events, cpu);
	struct perf_event *c_event = per_cpu(cycle_events, cpu);
	struct perf_event *l3_rf_event = per_cpu(l3dc_rf_events, cpu);

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
		if (!i_event && cpu >= boundary) {
			event = perf_event_create_kernel_counter(
				&inst_spec_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) inst_spec counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(inst_spec_events, cpu) = event;
		}
		if (!c_event && cpu >= boundary) {
			event = perf_event_create_kernel_counter(
				&cycle_event_attr, cpu,	NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) cycle counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(cycle_events, cpu) = event;
		}
		if (!l3_rf_event && pmu_rf_support && cpu >= rf_boundary) {
			event = perf_event_create_kernel_counter(
				&l3dc_rf_event_attr, cpu, NULL, NULL, NULL);
			if (IS_ERR(event)) {
				pr_notice("create (%d) l3dc_rf counter error (%d)\n",
					  cpu, (int)PTR_ERR(event));
				goto FAIL;
			}
			per_cpu(l3dc_rf_events, cpu) = event;
		}

		swpm_pmu_start(cpu);
	} else {
		swpm_pmu_stop(cpu);

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
		if (l3_rf_event) {
			per_cpu(l3dc_rf_events, cpu) = NULL;
			perf_event_release_kernel(l3_rf_event);
		}

	}

	return 0;
FAIL:
	return (int)PTR_ERR(event);
}

int swpm_arm_pmu_get_idx(unsigned int evt_id, unsigned int cpu)
{
	switch (evt_id) {
	case L3DC_EVT:
		return per_cpu(l3dc_idx, cpu);
	case INST_SPEC_EVT:
		return per_cpu(inst_spec_idx, cpu);
	case CYCLES_EVT:
		return per_cpu(cycle_idx, cpu);
	case L3DC_REFILL_EVT:
		return per_cpu(l3dc_rf_idx, cpu);
	}

	return -1;
}
EXPORT_SYMBOL(swpm_arm_pmu_get_idx);

int swpm_arm_pmu_get_status(void)
{
	return (pmu_rf_support << 28 |
		rf_boundary << 24 |
		boundary << 20 |
		swpm_arm_pmu_status);
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

int __init swpm_arm_pmu_init(void)
{
	int ret, i;
	struct device_node *node = NULL;
	char swpm_arm_pmu_desc[] = "mediatek,mtk-swpm";

	node = of_find_compatible_node(NULL, NULL, swpm_arm_pmu_desc);

	if (!node) {
		pr_notice("of_find_compatible_node unable to find swpm device node\n");
		goto END;
	}
	/* device node, device name, offset, variable */
	ret = of_property_read_u32_index(node, "pmu_boundary_num",
					 0, &boundary);
	if (ret) {
		pr_notice("failed to get pmu_boundary_num index from dts\n");
		goto END;
	}
	/* device node, device name, offset, variable */
	ret = of_property_read_u32_index(node, "pmu_rf_support",
					 0, &pmu_rf_support);
	if (ret) {
		pr_notice("failed to get pmu_rf_support index from dts\n");
		goto END;
	}

	/* rf half support */
	if (pmu_rf_support == 2)
		rf_boundary = boundary;
	else /* rf fully support */
		rf_boundary = 0;

END:
	for (i = 0; i < num_possible_cpus(); i++) {
		per_cpu(l3dc_idx, i) = -1;
		per_cpu(l3dc_rf_idx, i) = -1;
		per_cpu(inst_spec_idx, i) = -1;
		per_cpu(cycle_idx, i) = -1;
	}

	swpm_arm_pmu_enable_all(1);
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
