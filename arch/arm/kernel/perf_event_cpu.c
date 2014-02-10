/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2012 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */
#define pr_fmt(fmt) "CPU PMU: " fmt

#include <linux/bitmap.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/cpu_pm.h>
#include <linux/irq.h>

#include <asm/cputype.h>
#include <asm/irq_regs.h>
#include <asm/pmu.h>

/* Set at runtime when we know what CPU type we are. */
static struct arm_pmu *cpu_pmu;

static DEFINE_PER_CPU(u32, from_idle);
static DEFINE_PER_CPU(struct perf_event * [ARMPMU_MAX_HWEVENTS], hw_events);
static DEFINE_PER_CPU(unsigned long [BITS_TO_LONGS(ARMPMU_MAX_HWEVENTS)], used_mask);
static DEFINE_PER_CPU(struct pmu_hw_events, cpu_hw_events);
static DEFINE_PER_CPU(void *, pmu_irq_cookie);

/*
 * Despite the names, these two functions are CPU-specific and are used
 * by the OProfile/perf code.
 */
const char *perf_pmu_name(void)
{
	if (!cpu_pmu)
		return NULL;

	return cpu_pmu->name;
}
EXPORT_SYMBOL_GPL(perf_pmu_name);

int perf_num_counters(void)
{
	int max_events = 0;

	if (cpu_pmu != NULL)
		max_events = cpu_pmu->num_events;

	return max_events;
}
EXPORT_SYMBOL_GPL(perf_num_counters);

/* Include the PMU-specific implementations. */
#include "perf_event_xscale.c"
#include "perf_event_v6.c"
#include "perf_event_v7.c"
#include "perf_event_msm_krait.c"

static struct pmu_hw_events *cpu_pmu_get_cpu_events(void)
{
	return this_cpu_ptr(&cpu_hw_events);
}

void cpu_pmu_free_irq(struct arm_pmu *cpu_pmu)
{
	int i, irq, irqs;
	struct platform_device *pmu_device = cpu_pmu->plat_device;

	irqs = min(pmu_device->num_resources, num_possible_cpus());

	for (i = 0; i < irqs; ++i) {
		if (!cpumask_test_and_clear_cpu(i, &cpu_pmu->active_irqs))
			continue;
		irq = platform_get_irq(pmu_device, i);
		cpu_pmu->free_pmu_irq(irq, cpu_pmu);
	}
}

int cpu_pmu_request_irq(struct arm_pmu *cpu_pmu, irq_handler_t handler)
{
	int i, err, irq, irqs;
	struct platform_device *pmu_device = cpu_pmu->plat_device;

	if (!pmu_device)
		return -ENODEV;

	irqs = min(pmu_device->num_resources, num_possible_cpus());
	if (irqs < 1) {
		pr_err("no irqs for PMUs defined\n");
		return -ENODEV;
	}

	for (i = 0; i < irqs; ++i) {
		err = 0;
		irq = platform_get_irq(pmu_device, i);
		if (irq < 0)
			continue;

		/*
		 * If we have a single PMU interrupt that we can't shift,
		 * assume that we're running on a uniprocessor machine and
		 * continue. Otherwise, continue without this interrupt.
		 */
		if (irq_set_affinity(irq, cpumask_of(i)) && irqs > 1) {
			pr_warning("unable to set irq affinity (irq=%d, cpu=%u)\n",
				    irq, i);
			continue;
		}

		err = cpu_pmu->request_pmu_irq(irq, &handler, &cpu_pmu);
		if (err) {
			pr_err("unable to request IRQ%d for ARM PMU counters\n",
				irq);
			return err;
		}

		cpumask_set_cpu(i, &cpu_pmu->active_irqs);
	}

	return 0;
}

static void cpu_pmu_init(struct arm_pmu *cpu_pmu)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct pmu_hw_events *events = &per_cpu(cpu_hw_events, cpu);
		events->events = per_cpu(hw_events, cpu);
		events->used_mask = per_cpu(used_mask, cpu);
		events->from_idle = &per_cpu(from_idle, cpu);
		per_cpu(pmu_irq_cookie, cpu) = cpu_pmu;
		raw_spin_lock_init(&events->pmu_lock);
	}

	cpu_pmu->get_hw_events	= cpu_pmu_get_cpu_events;
	cpu_pmu->request_irq	= cpu_pmu_request_irq;
	cpu_pmu->free_irq	= cpu_pmu_free_irq;

	/* Ensure the PMU has sane values out of reset. */
	if (cpu_pmu->reset)
		on_each_cpu(cpu_pmu->reset, cpu_pmu, 1);
}

static int cpu_has_active_perf(int cpu)
{
	struct pmu_hw_events *hw_events;
	int enabled;

	if (!cpu_pmu)
		return 0;
	hw_events = &per_cpu(cpu_hw_events, cpu);
	enabled = bitmap_weight(hw_events->used_mask, cpu_pmu->num_events);

	if (enabled)
		/*Even one event's existence is good enough.*/
		return 1;

	return 0;
}

static void enable_irq_callback(void *info)
{
	int irq = *(unsigned int *)info;
	enable_percpu_irq(irq, IRQ_TYPE_EDGE_RISING);
}

static void disable_irq_callback(void *info)
{
	int irq = *(unsigned int *)info;
	disable_percpu_irq(irq);
}

static void armpmu_update_counters(void)
{
	struct pmu_hw_events *hw_events;
	int idx;

	if (!cpu_pmu)
		return;

	hw_events = cpu_pmu->get_hw_events();

	for (idx = 0; idx <= cpu_pmu->num_events; ++idx) {
		struct perf_event *event = hw_events->events[idx];

		if (!event)
			continue;

		cpu_pmu->pmu.read(event);
	}
}

/*
 * PMU hardware loses all context when a CPU goes offline.
 * When a CPU is hotplugged back in, since some hardware registers are
 * UNKNOWN at reset, the PMU must be explicitly reset to avoid reading
 * junk values out of them.
 */
static int __cpuinit cpu_pmu_notify(struct notifier_block *b,
				    unsigned long action, void *hcpu)
{
	int irq;
	struct pmu *pmu;
	int cpu = (int)hcpu;

	switch ((action & ~CPU_TASKS_FROZEN)) {
	case CPU_DOWN_PREPARE:
		if (cpu_pmu && cpu_pmu->save_pm_registers)
			smp_call_function_single(cpu,
						 cpu_pmu->save_pm_registers,
						 hcpu, 1);
		break;
	case CPU_STARTING:
		if (cpu_pmu && cpu_pmu->restore_pm_registers)
			smp_call_function_single(cpu,
						 cpu_pmu->restore_pm_registers,
						 hcpu, 1);
	}

	if (cpu_has_active_perf((int)hcpu)) {
		switch ((action & ~CPU_TASKS_FROZEN)) {

		case CPU_DOWN_PREPARE:
			armpmu_update_counters();
			/*
			 * If this is on a multicore CPU, we need
			 * to disarm the PMU IRQ before disappearing.
			 */
			if (cpu_pmu &&
				cpu_pmu->plat_device->dev.platform_data) {
				irq = platform_get_irq(cpu_pmu->plat_device, 0);
				smp_call_function_single((int)hcpu,
						disable_irq_callback, &irq, 1);
			}
			return NOTIFY_DONE;

		case CPU_STARTING:
			/*
			 * If this is on a multicore CPU, we need
			 * to arm the PMU IRQ before appearing.
			 */
			if (cpu_pmu &&
				cpu_pmu->plat_device->dev.platform_data) {
				irq = platform_get_irq(cpu_pmu->plat_device, 0);
				enable_irq_callback(&irq);
			}

			if (cpu_pmu && cpu_pmu->reset) {
				__get_cpu_var(from_idle) = 1;
				cpu_pmu->reset(NULL);
				pmu = &cpu_pmu->pmu;
				pmu->pmu_enable(pmu);
				return NOTIFY_OK;
			}
		default:
			return NOTIFY_DONE;
		}
	}



	if ((action & ~CPU_TASKS_FROZEN) != CPU_STARTING)
		return NOTIFY_DONE;

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata cpu_pmu_hotplug_notifier = {
	.notifier_call = cpu_pmu_notify,
};

/*TODO: Unify with pending patch from ARM */
static int perf_cpu_pm_notifier(struct notifier_block *self, unsigned long cmd,
		void *v)
{
	struct pmu *pmu;
	switch (cmd) {
	case CPU_PM_ENTER:
		if (cpu_pmu && cpu_pmu->save_pm_registers)
			cpu_pmu->save_pm_registers((void *)smp_processor_id());
		if (cpu_has_active_perf((int)v)) {
			armpmu_update_counters();
			pmu = &cpu_pmu->pmu;
			pmu->pmu_disable(pmu);
		}
		break;

	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		if (cpu_pmu && cpu_pmu->restore_pm_registers)
			cpu_pmu->restore_pm_registers(
				(void *)smp_processor_id());
		if (cpu_has_active_perf((int)v) && cpu_pmu->reset) {
			/*
			 * Flip this bit so armpmu_enable knows it needs
			 * to re-enable active counters.
			 */
			__get_cpu_var(from_idle) = 1;
			cpu_pmu->reset(NULL);
			pmu = &cpu_pmu->pmu;
			pmu->pmu_enable(pmu);
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block perf_cpu_pm_notifier_block = {
	.notifier_call = perf_cpu_pm_notifier,
};

/*
 * PMU platform driver and devicetree bindings.
 */
static struct of_device_id cpu_pmu_of_device_ids[] = {
	{.compatible = "arm,cortex-a15-pmu",	.data = armv7_a15_pmu_init},
	{.compatible = "arm,cortex-a9-pmu",	.data = armv7_a9_pmu_init},
	{.compatible = "arm,cortex-a8-pmu",	.data = armv7_a8_pmu_init},
	{.compatible = "arm,cortex-a7-pmu",	.data = armv7_a7_pmu_init},
	{.compatible = "arm,cortex-a5-pmu",	.data = armv7_a5_pmu_init},
	{.compatible = "arm,arm11mpcore-pmu",	.data = armv6mpcore_pmu_init},
	{.compatible = "arm,arm1176-pmu",	.data = armv6pmu_init},
	{.compatible = "arm,arm1136-pmu",	.data = armv6pmu_init},
	{.compatible = "qcom,krait-pmu",	.data = armv7_krait_pmu_init},
	{},
};

static struct platform_device_id cpu_pmu_plat_device_ids[] = {
	{.name = "arm-pmu"},
	{},
};

/*
 * CPU PMU identification and probing.
 */
static int probe_current_pmu(struct arm_pmu *pmu)
{
	int cpu = get_cpu();
	unsigned long implementor = read_cpuid_implementor();
	unsigned long part_number = read_cpuid_part_number();
	int ret = -ENODEV;

	pr_info("probing PMU on CPU %d\n", cpu);

	/* ARM Ltd CPUs. */
	if (implementor == ARM_CPU_IMP_ARM) {
		switch (part_number) {
		case ARM_CPU_PART_ARM1136:
		case ARM_CPU_PART_ARM1156:
		case ARM_CPU_PART_ARM1176:
			ret = armv6pmu_init(pmu);
			break;
		case ARM_CPU_PART_ARM11MPCORE:
			ret = armv6mpcore_pmu_init(pmu);
			break;
		case ARM_CPU_PART_CORTEX_A8:
			ret = armv7_a8_pmu_init(pmu);
			break;
		case ARM_CPU_PART_CORTEX_A9:
			ret = armv7_a9_pmu_init(pmu);
			break;
		case ARM_CPU_PART_CORTEX_A5:
			ret = armv7_a5_pmu_init(pmu);
			break;
		case ARM_CPU_PART_CORTEX_A15:
			ret = armv7_a15_pmu_init(pmu);
			break;
		case ARM_CPU_PART_CORTEX_A7:
			ret = armv7_a7_pmu_init(pmu);
			break;
		}
	/* Intel CPUs [xscale]. */
	} else if (implementor == ARM_CPU_IMP_INTEL) {
		switch (xscale_cpu_arch_version()) {
		case ARM_CPU_XSCALE_ARCH_V1:
			ret = xscale1pmu_init(pmu);
			break;
		case ARM_CPU_XSCALE_ARCH_V2:
			ret = xscale2pmu_init(pmu);
			break;
		}
	} else if (implementor == ARM_CPU_IMP_QUALCOMM) {
		switch (part_number) {
		case 0x04D0:    /* 8960 */
		case 0x06F0:    /* 8974, etc */
			ret = armv7_krait_pmu_init(pmu);
			break;
		}
	}

	put_cpu();
	return ret;
}


static int multicore_request_irq(int irq, irq_handler_t *handle_irq, void *dev_id)
{
	int err = 0;
	int cpu;

	err = request_percpu_irq(irq, *handle_irq, "l1-armpmu",
			&pmu_irq_cookie);

	if (!err) {
		for_each_cpu(cpu, cpu_online_mask) {
			smp_call_function_single(cpu,
					enable_irq_callback, &irq, 1);
		}
	}

	return err;
}

#ifdef CONFIG_SMP
static __ref int armpmu_cpu_up(int cpu)
{
	int ret = 0;

	if (!cpumask_test_cpu(cpu, cpu_online_mask)) {
		ret = cpu_up(cpu);
		if (ret)
			pr_err("Failed to bring up CPU: %d, ret: %d\n",
			       cpu, ret);
	}
	return ret;
}
#else
static inline int armpmu_cpu_up(int cpu)
{
	return 0;
}
#endif

static void __ref multicore_free_irq(int irq, void *dev_id)
{
	int cpu;
	struct irq_desc *desc = irq_to_desc(irq);

	if (irq >= 0) {
		for_each_cpu(cpu, desc->percpu_enabled) {
			if (!armpmu_cpu_up(cpu))
				smp_call_function_single(cpu,
						disable_irq_callback, &irq, 1);
		}
		free_percpu_irq(irq, &pmu_irq_cookie);
	}
}

struct arm_pmu_platdata multicore_data = {
	.request_pmu_irq = multicore_request_irq,
	.free_pmu_irq = multicore_free_irq,
};

static inline int get_dt_irq_prop(void)
{
	struct device_node *np = NULL;
	int err = -1;

	np = of_find_matching_node(NULL, cpu_pmu_of_device_ids);
	if (np)
		err = of_property_read_bool(np, "qcom,irq-is-percpu");
	else
		pr_err("Perf: can't find DT node.\n");

	return err;
}

static int cpu_pmu_device_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	int (*init_fn)(struct arm_pmu *);
	struct device_node *node = pdev->dev.of_node;
	struct arm_pmu *pmu;
	int ret = -ENODEV;

	if (cpu_pmu) {
		pr_info("attempt to register multiple PMU devices!");
		return -ENOSPC;
	}

	pmu = kzalloc(sizeof(struct arm_pmu), GFP_KERNEL);
	if (!pmu) {
		pr_info("failed to allocate PMU device!");
		return -ENOMEM;
	}

	if (node && (of_id = of_match_node(cpu_pmu_of_device_ids, pdev->dev.of_node))) {
		init_fn = of_id->data;
		ret = init_fn(pmu);
	} else {
		ret = probe_current_pmu(pmu);
	}

	if (ret) {
		pr_info("failed to probe PMU!");
		goto out_free;
	}

	cpu_pmu = pmu;
	cpu_pmu->plat_device = pdev;
	cpu_pmu_init(cpu_pmu);

	if (get_dt_irq_prop())
		cpu_pmu->plat_device->dev.platform_data = &multicore_data;

	ret = armpmu_register(cpu_pmu, PERF_TYPE_RAW);

	if (!ret)
		return 0;

out_free:
	pr_info("failed to register PMU devices!");
	kfree(pmu);
	return ret;
}

static struct platform_driver cpu_pmu_driver = {
	.driver		= {
		.name	= "arm-pmu",
		.pm	= &armpmu_dev_pm_ops,
		.of_match_table = cpu_pmu_of_device_ids,
	},
	.probe		= cpu_pmu_device_probe,
	.id_table	= cpu_pmu_plat_device_ids,
};

static int __init register_pmu_driver(void)
{
	int err;

	err = register_cpu_notifier(&cpu_pmu_hotplug_notifier);
	if (err)
		return err;

	err = cpu_pm_register_notifier(&perf_cpu_pm_notifier_block);
	if (err)
		goto err_cpu_pm;

	err = platform_driver_register(&cpu_pmu_driver);
	if (err)
		goto err_driver;

	return 0;

err_driver:
	cpu_pm_unregister_notifier(&perf_cpu_pm_notifier_block);
err_cpu_pm:
	unregister_cpu_notifier(&cpu_pmu_hotplug_notifier);
	return err;
}
device_initcall(register_pmu_driver);
