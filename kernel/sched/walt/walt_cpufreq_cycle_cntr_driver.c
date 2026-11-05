// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/topology.h>

#include "walt.h"

#define CYCLE_CNTR_OFFSET(core_id, acc_count)		\
				(acc_count ? ((core_id + 1) * 4) : 0)

struct cpufreq_counter {
	u64 total_cycle_counter;
	u32 prev_cycle_counter;
	spinlock_t lock;
};

static struct cpufreq_counter walt_cpufreq_counter[NR_CPUS];

struct walt_cpufreq_soc_data {
	u32 reg_enable;
	u32 reg_cycle_cntr;
	bool accumulative_counter;
};

struct walt_cpufreq_data {
	void __iomem *base;
	const struct walt_cpufreq_soc_data *soc_data;
};

static struct walt_cpufreq_data cpufreq_data[MAX_CLUSTERS];

u64 walt_cpufreq_get_cpu_cycle_counter(int cpu, u64 wc)
{
	const struct walt_cpufreq_soc_data *soc_data;
	struct cpufreq_counter *cpu_counter;
	struct walt_cpufreq_data *data;
	u64 cycle_counter_ret;
	unsigned long flags;
	u16 offset;
	u32 val;

	data = &cpufreq_data[cpu_cluster(cpu)->id];
	soc_data = data->soc_data;

	cpu_counter = &walt_cpufreq_counter[cpu];
	spin_lock_irqsave(&cpu_counter->lock, flags);

	offset = CYCLE_CNTR_OFFSET(topology_core_id(cpu),
					soc_data->accumulative_counter);
	val = readl_relaxed(data->base +
					soc_data->reg_cycle_cntr + offset);

	if (val < cpu_counter->prev_cycle_counter) {
		/* Handle counter overflow */
		cpu_counter->total_cycle_counter += UINT_MAX -
			cpu_counter->prev_cycle_counter + val;
		cpu_counter->prev_cycle_counter = val;
	} else {
		cpu_counter->total_cycle_counter += val -
			cpu_counter->prev_cycle_counter;
		cpu_counter->prev_cycle_counter = val;
	}
	cycle_counter_ret = cpu_counter->total_cycle_counter;
	spin_unlock_irqrestore(&cpu_counter->lock, flags);

	pr_debug("CPU %u, core-id 0x%x, offset %u cycle_counts=%llu\n",
			cpu, topology_core_id(cpu), offset, cycle_counter_ret);

	return cycle_counter_ret;
}

static int walt_cpufreq_cycle_cntr_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct of_phandle_args args;
	struct device_node *cpu_np;
	void __iomem *base;
	int ret = -ENODEV, index, cpu;
	struct walt_sched_cluster *cluster;

	for_each_sched_cluster(cluster) {
		cpu = cluster_first_cpu(cluster);
		cpu_np = of_cpu_device_node_get(cpu);
		if (!cpu_np)
			return -EINVAL;

		ret = of_parse_phandle_with_args(cpu_np, "qcom,freq-domain",
					 "#freq-domain-cells", 0, &args);
		of_node_put(cpu_np);
		if (ret)
			return ret;

		index = args.args[0];

		res = platform_get_resource(pdev, IORESOURCE_MEM, index);
		if (!res) {
			dev_err(dev, "failed to get mem resource %d\n", index);
			return -ENODEV;
		}

		base = devm_ioremap(dev, res->start, resource_size(res));
		if (!base) {
			dev_err(dev, "failed to map resource %pR\n", res);
			return -ENOMEM;
		}

		cpufreq_data[cluster->id].soc_data = of_device_get_match_data(&pdev->dev);
		cpufreq_data[cluster->id].base = base;

		/* HW should be in enabled state to proceed */
		if (!(readl_relaxed(base + cpufreq_data[cluster->id].soc_data->reg_enable) & 0x1)) {
			dev_err(dev, "Domain-%d cpufreq hardware not enabled\n", index);
			return -ENODEV;
		}
	}

	if (!walt_get_cycle_counts_cb) {
		for_each_possible_cpu(cpu)
			spin_lock_init(&walt_cpufreq_counter[cpu].lock);
		walt_get_cycle_counts_cb = walt_cpufreq_get_cpu_cycle_counter;
		use_cycle_counter = true;
		complete(&walt_get_cycle_counts_cb_completion);

		return 0;
	}

	return ret;
}

static int walt_cpufreq_cycle_cntr_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct walt_cpufreq_soc_data hw_soc_data = {
	.reg_enable = 0x0,
	.reg_cycle_cntr = 0x9c0,
	.accumulative_counter = false,
};

static const struct walt_cpufreq_soc_data epss_soc_data = {
	.reg_enable = 0x0,
	.reg_cycle_cntr = 0x3c4,
	.accumulative_counter = true,
};

static const struct of_device_id walt_cpufreq_cycle_cntr_match[] = {
	{ .compatible = "qcom,cycle-cntr-hw", .data = &hw_soc_data },
	{ .compatible = "qcom,epss", .data = &epss_soc_data },
	{}
};

static struct platform_driver walt_cpufreq_cycle_cntr_driver = {
	.driver = {
		.name = "walt-cpufreq-cycle-cntr",
		.of_match_table = walt_cpufreq_cycle_cntr_match
	},
	.probe = walt_cpufreq_cycle_cntr_driver_probe,
	.remove = walt_cpufreq_cycle_cntr_driver_remove,
};

int walt_cpufreq_cycle_cntr_driver_register(void)
{
	return platform_driver_register(&walt_cpufreq_cycle_cntr_driver);
}
