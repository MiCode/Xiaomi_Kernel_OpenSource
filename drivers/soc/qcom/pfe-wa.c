/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/perf_event.h>
#include <linux/slab.h>

/*
 * BIT 19:16 -> prefix
 * BIT 15:12 -> Reg number
 * BIT 11:4  -> Code number
 * BIT  3:0  -> Group number
 */
#define L1_DROP_EVENT 0x12513

#define EHWPF_BITS 0x3
#define EHWPF_SHIFT 0x0
#define EHWPF_MASK ~(EHWPF_BITS << EHWPF_SHIFT)

/*
 * EHWPF
 * 0x0: turn off PFE
 * 0x1: turn on l1 cache PFE
 * 0x2: turn on l1 and l2 cache PFE
 * 0x3: turn on l1,l2, l3 cache PFE
 */
#define EHWPF_OFF  (0x0 << EHWPF_SHIFT)
#define EHWPF_L1L2 (0x2 << EHWPF_SHIFT)

struct l1_drop_data {
	struct notifier_block nb_cpu;
	struct perf_event *l1_drop_events[NR_CPUS];
};

static unsigned long max_allowed_cntr = 257;
module_param(max_allowed_cntr, ulong, 0);

static void l1_drop_handler(struct perf_event *event,
		struct perf_sample_data *data,
		struct pt_regs *regs)
{
	u32 val;

	asm volatile ("mrs %0, s3_1_c11_c4_7" : "=r" (val));
	val = (val & EHWPF_MASK) | EHWPF_OFF;
	asm volatile ("msr s3_1_c11_c4_7, %0" : : "r" (val));
	val = (val & EHWPF_MASK) | EHWPF_L1L2;
	isb();
	asm volatile ("msr s3_1_c11_c4_7, %0" : : "r" (val));
	isb();
}

static void l1_drop_event_create(int cpu, void *info)
{
	struct l1_drop_data *drv = info;
	struct perf_event *event = drv->l1_drop_events[cpu];
	struct perf_event_attr attr = {
		.pinned = 1,
		.disabled = 0,
		.sample_period = max_allowed_cntr,
		.type = PERF_TYPE_RAW,
		.config = L1_DROP_EVENT,
		.size = sizeof(struct perf_event_attr),
	};

	if (event)
		return;
	event = perf_event_create_kernel_counter(&attr, cpu, NULL,
			l1_drop_handler,
			drv);
	if (IS_ERR(event)) {
		pr_err("PERF Event creation failed on cpu %d ptr_err %ld\n",
				cpu, PTR_ERR(event));
		return;
	}

	drv->l1_drop_events[cpu] = event;
}

static int l1_drop_cpu_notify(struct notifier_block *self,
		unsigned long action, void *hcpu)
{
	struct l1_drop_data *data = container_of(self, struct l1_drop_data,
					nb_cpu);
	unsigned long cpu = (unsigned long)hcpu;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_ONLINE:
		l1_drop_event_create(cpu, data);
		break;
	};

	return NOTIFY_OK;
}

static struct of_device_id pfe_wa_of_match[] = {
	{.compatible = "qcom,kryo-pmuv3", },
	{}
};

static int __init msm_pfe_wa_init(void)
{
	int cpu;
	struct device_node *np;
	struct l1_drop_data *l1_drop;

	np = of_find_matching_node(NULL, pfe_wa_of_match);
	if (!np) {
		pr_debug("Disable PFE WA - PMU is not enabled\n");
		return -ENODEV;
	}
	if (!of_find_property(np, "qcom,enable-pfe-wa", NULL)) {
		pr_debug("Disable PFE WA\n");
		return -ENODEV;
	}

	l1_drop = kzalloc(sizeof(struct l1_drop_data), GFP_KERNEL);
	if (!l1_drop)
		return -ENOMEM;

	l1_drop->nb_cpu.notifier_call = l1_drop_cpu_notify;
	register_cpu_notifier(&l1_drop->nb_cpu);
	get_online_cpus();
	for_each_online_cpu(cpu)
		l1_drop_event_create(cpu, l1_drop);
	put_online_cpus();

	return 0;
}

late_initcall(msm_pfe_wa_init);
